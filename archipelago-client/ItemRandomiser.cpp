#include "Core.h"
#include "GameHook.h"
#include <functional>

extern CCore* Core;
extern CItemRandomiser* ItemRandomiser;

VOID fItemRandomiser(UINT_PTR qWorldChrMan, UINT_PTR pItemBuffer, UINT_PTR pItemData, DWORD64 qReturnAddress) {

	if (*(int*)(pItemData) >= 0) ItemRandomiser->RandomiseItem(qWorldChrMan, pItemBuffer, pItemData, qReturnAddress);

	return;
}


VOID CItemRandomiser::RandomiseItem(UINT_PTR qWorldChrMan, UINT_PTR pItemBuffer, UINT_PTR pItemData, DWORD64 qReturnAddress) {

	DWORD dItemAmount = 0;
	DWORD dItemID = 0;
	DWORD dItemQuantity = 0;
	DWORD dItemDurability = 0;

	dItemAmount = *(int*)pItemBuffer;
	pItemBuffer += 4;

	if (dItemAmount > 6) {
		Core->Panic("Too many items!", "...\\Source\\ItemRandomiser\\ItemRandomiser.cpp", FE_AmountTooHigh, 1);
		int3
	};

	while (dItemAmount) {

		dItemID = *(int*)(pItemBuffer);
		dItemQuantity = *(int*)(pItemBuffer + 0x04);
		dItemDurability = *(int*)(pItemBuffer + 0x08);

		//Make some checks about the item picked by the player
		int serverLocationIndex = -1;
		int locationTargetItem = 0;

		//Check if the item is received from the server
		if (isReceivedFromServer(dItemID)) {
			receivedItemsQueue.pop_back();
			//Nothing to do, just let the item go to the player's inventory
		} else if ((serverLocationIndex = isARandomizedLocation(dItemID)) != -1) { //Check if the item is a randomized location
			//From here, the item is considered as a location!	
			//Check if the location contains a item for the local player
			
			if ((locationTargetItem = pLocationsTarget[serverLocationIndex]) != 0) {
				dItemID = locationTargetItem;
			} else {
				//The item is for another player, give a Prism shard
				dItemID = 0x40000172;
			}

			checkedLocationsList.push_front(pLocationsId[serverLocationIndex]);

		} else { 
			//Nothing to do, this is a vanilla item so we will let it go to the player's inventory	
		}

		*(int*)(pItemBuffer) = dItemID;
		*(int*)(pItemBuffer + 0x04) = dItemQuantity;
		*(int*)(pItemBuffer + 0x08) = -1;

		dItemAmount--;
		pItemBuffer += 0x0C;
	};

	return;
}

int CItemRandomiser::isARandomizedLocation(DWORD dItemID) {
	for (int i = 0; i < pLocationsAddress.size(); ++i) {
		if (dItemID == pLocationsAddress[i]) {
			return i;
		}
	}
	return -1;
}

BOOL CItemRandomiser::isReceivedFromServer(DWORD dItemID) {
	for (DWORD item : receivedItemsQueue) {
		if (dItemID == item) {
			return true;
		}
	}
	return false;
}