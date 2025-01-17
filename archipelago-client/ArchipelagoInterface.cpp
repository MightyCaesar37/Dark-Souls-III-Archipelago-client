#include "ArchipelagoInterface.h"

#ifdef __EMSCRIPTEN__
#define DATAPACKAGE_CACHE "/settings/datapackage.json"
#define UUID_FILE "/settings/uuid"
#else
#define DATAPACKAGE_CACHE "datapackage.json" // TODO: place in %appdata%
#define UUID_FILE "uuid" // TODO: place in %appdata%
#endif

extern CCore* Core;
extern CItemRandomiser* ItemRandomiser;
extern CGameHook* GameHook;

bool ap_sync_queued = false;
APClient* ap;

BOOL CArchipelago::Initialise(std::string URI) {
	
	// read or generate uuid, required by AP
	std::string uuid = ap_get_uuid(UUID_FILE);
	ap = new APClient(uuid, "Dark Souls III", URI);

	ap_sync_queued = false;
	ap->set_socket_connected_handler([]() {
		});
	ap->set_socket_disconnected_handler([]() {
		});
	ap->set_slot_connected_handler([](const json& data) {
		//std::cout << data.dump() << std::endl;				//The slot_data feature is not ready yet
		});
	ap->set_slot_disconnected_handler([]() {
		});

	ap->set_room_info_handler([]() {
		std::list<std::string> tags;
		if (GameHook->dIsDeathLink) { tags.push_back("DeathLink"); }
		ap->ConnectSlot(Core->pSlotName, "", 1, tags, { 0,3,4 });
		});

	ap->set_items_received_handler([](const std::list<APClient::NetworkItem>& items) {
		
		if (!ap->is_data_package_valid()) {
			// NOTE: this should not happen since we ask for data package before connecting
			if (!ap_sync_queued) ap->Sync();
			ap_sync_queued = true;
			return;
		}

		for (const auto& item : items) {
			std::string itemname = ap->get_item_name(item.item);
			std::string sender = ap->get_player_alias(item.player);
			std::string location = ap->get_location_name(item.location);

			//Check if we should ignore this item
			if (item.index < Core->pLastReceivedIndex) {
				continue;
			}

			std::ostringstream stringStream;
			stringStream << "#" << item.index << ": " << itemname.c_str() << " from " << sender.c_str() << " - " << location.c_str() << std::endl;
			std::string itemDesc = stringStream.str();

			//Add the item to the list of already received items, only for logging purpose
			Core->pReceivedItems.push_back(itemDesc);
			printf(itemDesc.c_str());

			//Determine the item address
			DWORD address = 0;
			for (int i = 0; i < ItemRandomiser->pItemsId.size(); i++) {
				if (ItemRandomiser->pItemsId[i] == item.item) {
					address = ItemRandomiser->pItemsAddress[i];
					break;
				}
			}
			if (address == 0) {
				std::cout << "items_received_handler " << itemname.c_str() << " not found" << "\n";
				return;
			}

			ItemRandomiser->receivedItemsQueue.push_front((DWORD)address);
		}
		});

	ap->set_data_package_changed_handler([](const json& data) {
		ap->save_data_package(DATAPACKAGE_CACHE);
		});

	ap->set_print_handler([](const std::string& msg) {
		printf("%s\n", msg.c_str());
		});

	ap->set_print_json_handler([](const std::list<APClient::TextNode>& msg) {
		printf("%s\n", ap->render_json(msg, APClient::RenderFormat::TEXT).c_str());
		});

	ap->set_bounced_handler([](const json& cmd) {
		if (GameHook->dIsDeathLink) {
			auto tagsIt = cmd.find("tags");
			auto dataIt = cmd.find("data");
			if (tagsIt != cmd.end() && tagsIt->is_array()
				&& std::find(tagsIt->begin(), tagsIt->end(), "DeathLink") != tagsIt->end())
			{
				if (dataIt != cmd.end() && dataIt->is_object()) {
					json data = *dataIt;
					if (data["source"].get<std::string>() != Core->pSlotName) {
						printf("Died by the hands of %s: %s\n",
							data["source"].is_string() ? data["source"].get<std::string>().c_str() : "???",
							data["cause"].is_string() ? data["cause"].get<std::string>().c_str() : "???");
						GameHook->deathLinkData = true;
					}
				}
				else {
					printf("Bad deathlink packet!\n");
				}
			}
		}
		});
	
	return true;
}

VOID CArchipelago::say(std::string message) {
	if (ap && ap->get_state() == APClient::State::SLOT_CONNECTED) {
		ap->Say(message);
	}
}

VOID CArchipelago::update() {
	if (ap) ap->poll();

	if (ap && !ItemRandomiser->checkedLocationsList.empty()) {
		if (ap->LocationChecks(ItemRandomiser->checkedLocationsList)) {
			ItemRandomiser->checkedLocationsList.clear();
		}
	}
}

VOID CArchipelago::gameFinished() {
	if (ap) ap->StatusUpdate(APClient::ClientStatus::GOAL);
}

VOID CArchipelago::sendDeathLink() {
	if (!ap || !GameHook->dIsDeathLink) return;

	printf("Sending deathlink...\n");

	json data{
		{"time", ap->get_server_time()},
		{"cause", "Dark Souls III."},
		{"source", ap->get_slot()},
	};
	ap->Bounce(data, {}, {}, { "DeathLink" });
}