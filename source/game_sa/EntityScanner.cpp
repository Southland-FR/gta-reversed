#include "StdInc.h"

#include "EntityScanner.h"

// Forward declaration of implementation
static void ScanForEntitiesInRange_Impl(CEntityScanner* self, eRepeatSectorList sectorList, const CPed& ped);

// Raw __thiscall hook function - bypasses member function vtable issues
// __fastcall with dummy EDX parameter simulates __thiscall
static void __fastcall ScanForEntitiesInRange_Hook(CEntityScanner* self, void* /*edx*/, eRepeatSectorList sectorList, const CPed* ped) {
    // Call implementation directly, not through vtable
    ScanForEntitiesInRange_Impl(self, sectorList, *ped);
}

void CEntityScanner::InjectHooks() {
    RH_ScopedClass(CEntityScanner);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Clear, 0x5FF9D0);
    // Hook using raw function to control calling convention
    ReversibleHooks::Install("Global/CEntityScanner", "ScanForEntitiesInRange", 0x5FFA20, ScanForEntitiesInRange_Hook);
}

// 0x5FF990
CEntityScanner::CEntityScanner() {
    field_4 = 0;
    {
        m_nCount = 0;
        std::ranges::fill(m_apEntities, nullptr);
    }
    m_pClosestEntityInRange = nullptr;
    m_nCount = 16;
}

// 0x603480
CEntityScanner::~CEntityScanner() {
    Clear();
}

// 0x5FF9D0
void CEntityScanner::Clear() {
    for (auto& entity : m_apEntities) {
        if (entity) {
            entity->CleanUpOldReference(&entity);
            entity = nullptr;
        }
    }
    if (m_pClosestEntityInRange) {
        m_pClosestEntityInRange->CleanUpOldReference(&m_pClosestEntityInRange);
        m_pClosestEntityInRange = nullptr;
    }
}

// 0x5FFA20
static void ScanForEntitiesInRange_Impl(CEntityScanner* self, eRepeatSectorList sectorList, const CPed& ped) {
    int32 oldField4 = self->field_4;
    self->field_4++;

    if ((int32)self->m_nCount < self->field_4) {
        self->field_4 = 0;
    }

    if (oldField4 != 0) {
        return;
    }

    // Clear existing entities
    self->Clear();

    // Get range from ped intelligence
    CPedIntelligence* intel = ped.m_pIntelligence;
    float range;
    if (intel->m_fHearingRange <= intel->m_fSeeingRange) {
        range = intel->m_fSeeingRange;
    } else {
        range = intel->m_fHearingRange;
    }

    const CVector& pedPos = ped.GetPosition();

    // Local variables
    int32 count = 0;
    float distances[16];
    for (int32 i = 0; i < 16; i++) {
        distances[i] = 3.4028235e+38f;
    }

    // Sector bounds
    int32 minX = CWorld::GetSectorX(pedPos.x - range);
    if (minX < 0) minX = 0;

    int32 minY = CWorld::GetSectorY(pedPos.y - range);
    if (minY < 0) minY = 0;

    int32 maxX = CWorld::GetSectorX(pedPos.x + range);
    if (maxX > MAX_SECTORS_X - 1) maxX = MAX_SECTORS_X - 1;

    int32 maxY = CWorld::GetSectorY(pedPos.y + range);
    if (maxY > MAX_SECTORS_Y - 1) maxY = MAX_SECTORS_Y - 1;

    // Scan code
    if (CWorld::ms_nCurrentScanCode == (uint16)-1) {
        CWorld::ClearScanCodes();
        CWorld::ms_nCurrentScanCode = 1;
    } else {
        ++CWorld::ms_nCurrentScanCode;
    }
    const_cast<CPed&>(ped).SetScanCode(CWorld::ms_nCurrentScanCode);

    CPed* playerPed = FindPlayerPed();
    float rangeSq = range * range;

    // Scan sectors
    for (int32 y = minY; y <= maxY; y++) {
        for (int32 x = minX; x <= maxX; x++) {
            CRepeatSector* sector = GetRepeatSector(x, y);

            // Get list based on type
            void* listPtr = nullptr;
            switch (sectorList) {
            case REPEATSECTOR_VEHICLES:
                listPtr = sector->Vehicles.GetNode();
                break;
            case REPEATSECTOR_PEDS:
                listPtr = sector->Peds.GetNode();
                break;
            case REPEATSECTOR_OBJECTS:
                listPtr = sector->Objects.GetNode();
                break;
            }

            auto* node = reinterpret_cast<CPtrNodeDoubleLink<CEntity*>*>(listPtr);
            while (node != nullptr) {
                CEntity* entity = node->Item;
                node = node->Next;

                if (entity->GetScanCode() == CWorld::ms_nCurrentScanCode) {
                    continue;
                }
                entity->SetScanCode(CWorld::ms_nCurrentScanCode);

                // Skip dead peds for NPC scanners
                if (sectorList == REPEATSECTOR_PEDS && playerPed != &ped) {
                    if (entity->AsPed()->m_nPedState == PEDSTATE_DEAD) {
                        continue;
                    }
                }

                const CVector& entPos = entity->GetPosition();
                float dx = entPos.x - pedPos.x;
                float dy = entPos.y - pedPos.y;
                float dz = entPos.z - pedPos.z;
                float distSq = dx*dx + dy*dy + dz*dz;

                if (distSq >= rangeSq) {
                    continue;
                }

                // Insert sorted
                int32 insertPos = 0;
                while (insertPos < 16) {
                    if (self->m_apEntities[insertPos] == nullptr) {
                        distances[insertPos] = distSq;
                        self->m_apEntities[insertPos] = entity;
                        count++;
                        goto next_entity;
                    }
                    if (distSq < distances[insertPos]) {
                        break;
                    }
                    insertPos++;
                }

                if (insertPos < 16) {
                    int32 shiftPos = (count < 16) ? count : 15;
                    while (shiftPos > insertPos) {
                        self->m_apEntities[shiftPos] = self->m_apEntities[shiftPos - 1];
                        distances[shiftPos] = distances[shiftPos - 1];
                        shiftPos--;
                    }
                    distances[insertPos] = distSq;
                    self->m_apEntities[insertPos] = entity;
                    if (count < 16) {
                        count++;
                    }
                }

                next_entity:;
            }
        }
    }

    // Register references
    for (int32 i = 0; i < count; i++) {
        if (self->m_apEntities[i] != nullptr) {
            self->m_apEntities[i]->RegisterReference(&self->m_apEntities[i]);
        }
    }

    if (self->m_apEntities[0] != nullptr) {
        self->m_pClosestEntityInRange = self->m_apEntities[0];
        self->m_pClosestEntityInRange->RegisterReference(&self->m_pClosestEntityInRange);
    }
}

// Member function stub (not used when hooked)
void CEntityScanner::ScanForEntitiesInRange(eRepeatSectorList sectorList, const CPed& ped) {
    ScanForEntitiesInRange_Impl(this, sectorList, ped);
}
