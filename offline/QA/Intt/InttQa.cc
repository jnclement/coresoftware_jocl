#include "InttQa.h"

// #include <ffarawobjects/Gl1Packet.h>
// #include <trackbase/InttEventHeader.h> // why in trackbase and not ffarawobjects??????????
#include <ffarawobjects/InttRawHit.h>
#include <ffarawobjects/InttRawHitContainer.h>
#include <intt/InttMapping.h>

#include <fun4all/Fun4AllHistoManager.h>  // required by QAHistManagerDef
#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/Fun4AllServer.h>

#include <phool/PHCompositeNode.h>
#include <phool/PHPointerListIterator.h>
#include <phool/getClass.h>
#include <phool/phool.h>  // PHWHERE

#include <qautils/QAHistManagerDef.h>

#include <TH1D.h>
#include <TH2D.h>

#include <format> // -std=c++20
#include <boost/format.hpp> // std::format,vformat are a pain to use with non-const strings
#include <exception>
#include <vector>

InttQa::InttQa (
	std::string const& name
) : SubsysReco(name) {

	auto* hm = QAHistManagerDef::getHistoManager();

	// The numeric values are important for naming the histograms,
	// which is why I do not use range-based for loops here
	for (int felix_server = 0; felix_server < n_felix_servers; ++felix_server) {

		m_felix_server_hit_distribution[felix_server] = new TH2D (
			std::format("{}_hit_distribution_server{:01d}", m_prefix, felix_server).c_str(),
			std::format("Hit Distribution intt{:01d}", felix_server).c_str(),
			n_chips, -0.5, n_chips-0.5,
			n_felix_channels, -0.5, n_felix_channels-0.5
		);
		hm->registerHisto(m_felix_server_hit_distribution[felix_server]);

		//...

		for (int felix_channel = 0; felix_channel < n_felix_channels; ++felix_channel) {

			m_felix_channel_bco_distribution[felix_server][felix_channel] = new TH1D (
				std::format("{}_bco_distribution_server{:01d}_channel{:02d}", m_prefix, felix_server, felix_channel).c_str(),
				std::format("BCO Distribution intt{:01d}, Felix Channel {:02d}", felix_server, felix_channel).c_str(),
				n_bcos, -0.5, n_bcos-0.5
			);
			hm->registerHisto(m_felix_channel_bco_distribution[felix_server][felix_channel]);

			m_felix_channel_hit_distribution[felix_server][felix_channel] = new TH2D (
				std::format("{}_hit_distribution_server{:01d}_channel{:02d}", m_prefix, felix_server, felix_channel).c_str(),
				std::format("Hit Distribution intt{:01d}, Felix Channel {:02d}", felix_server, felix_channel).c_str(),
				n_channels, -0.5, n_channels-0.5,
				n_chips, -0.5, n_chips-0.5
			);
			hm->registerHisto(m_felix_channel_hit_distribution[felix_server][felix_channel]);

			// ...

			for (int chip = 0; chip < n_chips; ++chip) {

				m_chip_hit_distribution[felix_server][felix_channel][chip] = new TH1D (
					std::format("{}_hit_distribution_server{:01d}_channel{:02d}_chip{:02d}", m_prefix, felix_server, felix_channel, chip).c_str(),
					std::format("Hit Distribution intt{:01d}, Felix Channel {:02d}, Chip{:02d}", felix_server, felix_channel, chip).c_str(),
					n_bcos, -0.5, n_bcos-0.5
				);
				hm->registerHisto(m_chip_hit_distribution[felix_server][felix_channel][chip]);

				m_chip_adc_distribution[felix_server][felix_channel][chip] = new TH1D (
					std::format("{}_adc_distribution_server{:01d}_channel{:02d}_chip{:02d}", m_prefix, felix_server, felix_channel, chip).c_str(),
					std::format("ADC Distribution intt{:01d}, Felix Channel {:02d}, Chip{:02d}", felix_server, felix_channel, chip).c_str(),
					n_adcs, -0.5, n_adcs-0.5
				);
				hm->registerHisto(m_chip_adc_distribution[felix_server][felix_channel][chip]);

				//...
			}
		}
	}

	for (int barrel = 0; barrel < n_barrels; ++barrel) {

		m_barrel_hit_distribution[barrel] = new TH2D (
			std::format("{}_hit_distribution_barrel_{:01d}", m_prefix, barrel).c_str(),
			std::format("Hit Distribution {} Barrel", (barrel ? "Outer" : "Inner")).c_str(),
			n_chips, -0.5, n_chips-0.5,
			n_ladders[barrel], -3.1416 * (1.0 + 1.0 / n_ladders[barrel]), +3.1416 * (1.0 - 1.0 / n_ladders[barrel])
		);

		hm->registerHisto(m_barrel_hit_distribution[barrel]);
	}
}

int
InttQa::InitRun (
	PHCompositeNode* top_node
) {
	// In case this is called multiple times per instance lifetime
	m_intt_raw_hit_containers.clear();

	PHNodeIterator dst_itr(top_node);

	PHCompositeNode* intt_node = dynamic_cast<PHCompositeNode*>(dst_itr.findFirst("INTT"));
	if (!intt_node) {
		if (Verbosity()) {
			std::cout
				<< PHWHERE
				<< "\tNo 'INTT' node found on the NodeTree, Unregistering\n"
				<< std::flush;
		}
		Fun4AllServer::instance()->unregisterSubsystem(this);
		return Fun4AllReturnCodes::EVENT_OK;
	}
	PHNodeIterator intt_itr(intt_node);

	// For the record, if you're reading this, I do not like this loop structure either
	// I think this is moot b/c in practice most DST will only have one node, 'INTTRAWHIT'
	PHPointerListIterator<PHNode> next_intt_node(intt_itr.ls());
	for (PHNode* itr_node; (itr_node = next_intt_node());) {
		auto* intt_raw_hit_container_node = static_cast<PHIODataNode<InttRawHitContainer>*>(itr_node);
		if (!intt_raw_hit_container_node) { continue; }
		if (Verbosity()) {
			std::cout
				<< PHWHERE
				<< "Got InttRawHitContainerNode: '" << intt_raw_hit_container_node->getName() << "'"
				<< std::endl;
		}

		auto* intt_raw_hit_container = dynamic_cast<InttRawHitContainer*>(intt_raw_hit_container_node->getData());
		if (!intt_raw_hit_container) { continue; }
		m_intt_raw_hit_containers.push_back(intt_raw_hit_container);
	}

	if (m_intt_raw_hit_containers.empty()) {
		if (Verbosity()) {
			std::cout
				<< PHWHERE
				<< "No InttRawHitContainers found on the NodeTree, Unregistering"
				<< std::endl;
		}
		Fun4AllServer::instance()->unregisterSubsystem(this);
		return Fun4AllReturnCodes::EVENT_OK;
	}

	return Fun4AllReturnCodes::EVENT_OK;
}

int
InttQa::process_event (
	PHCompositeNode* /*unused*/
) {
	for (auto const& intt_raw_hit_container : m_intt_raw_hit_containers) {
		for (unsigned int hit_index{0}; hit_index < intt_raw_hit_container->get_nhits(); ++hit_index) {

			auto* hit = intt_raw_hit_container->get_hit(hit_index);
			InttNameSpace::RawData_s raw = InttNameSpace::RawFromHit(hit);

			// Fine for triggered case, for streaming we will need the GL1
			// (and a usage of InttOdbcQuery to check if the data is streaming)
			// int bco_diff = hit->get_FPHX_BCO() + hit->get_bco() - gl1_bco 
			int bco_diff = (hit->get_FPHX_BCO() - int(hit->get_bco() & 0x7FU) + n_bcos) % n_bcos;
			int adc = hit->get_adc();

			m_felix_server_hit_distribution[raw.felix_server]->Fill(raw.chip, raw.felix_channel);
			m_felix_channel_bco_distribution[raw.felix_server][raw.felix_channel]->Fill(bco_diff);
			m_felix_channel_hit_distribution[raw.felix_server][raw.felix_channel]->Fill(raw.channel, raw.chip);
			m_chip_hit_distribution[raw.felix_server][raw.felix_channel][raw.chip]->Fill(raw.channel);
			m_chip_adc_distribution[raw.felix_server][raw.felix_channel][raw.chip]->Fill(adc);

			InttNameSpace::Offline_s offline = InttNameSpace::ToOffline(raw);
			int layer = offline.layer - 3;
			int barrel = layer / 2;

			// Every other layer is staggered
			double phi = 6.2832 * (2.0 * offline.ladder_phi - (layer % 2)) / n_ladders[barrel];
			while (3.1416 * (1.0 - 1.0 / n_ladders[barrel]) < phi) { phi -= 6.2832; }

			double z_index = offline.strip_y;
			switch (offline.ladder_z) {
			case 0:
				z_index += 5;
				break;
			case 2:
				z_index += 13;
				break;
			case 3:
				z_index += 21;
				break;
			default:
				break;
			}
			m_barrel_hit_distribution[barrel]->Fill(z_index, phi);
		}
	}

	return Fun4AllReturnCodes::EVENT_OK;
}

