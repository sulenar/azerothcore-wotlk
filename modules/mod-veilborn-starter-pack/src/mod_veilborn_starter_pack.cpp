#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "LootMgr.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace VeilbornStarterPack
{
    constexpr uint32 STARTER_PACK_ITEM = 900001;
    constexpr uint32 XP_POTION_ITEM = 900002;

    constexpr uint32 DEFAULT_MIN_LEVEL = 10;
    constexpr uint32 DEFAULT_POTION_COUNT = 5;
    constexpr uint32 DEFAULT_GOLD = 100;
    constexpr uint32 DEFAULT_POTION_DURATION = 60 * 60;
    constexpr uint32 DEFAULT_XP_MULTIPLIER = 2;

    uint32 GetMinLevel()
    {
        return sConfigMgr->GetOption<uint32>(
            "Veilborn.StarterPack.MinLevel",
            DEFAULT_MIN_LEVEL
        );
    }

    uint32 GetPotionDuration()
    {
        return sConfigMgr->GetOption<uint32>(
            "Veilborn.StarterPack.PotionDuration",
            DEFAULT_POTION_DURATION
        );
    }

    uint32 GetGold()
    {
        return sConfigMgr->GetOption<uint32>(
            "Veilborn.StarterPack.Gold",
            DEFAULT_GOLD
        );
    }

    uint32 GetXpMultiplier()
    {
        return std::max(
            1u,
            sConfigMgr->GetOption<uint32>(
                "Veilborn.StarterPack.XpMultiplier",
                DEFAULT_XP_MULTIPLIER
            )
        );
    }

    bool IsEnabled()
    {
        return sConfigMgr->GetOption<bool>(
            "Veilborn.StarterPack.Enable",
            true
        );
    }

    // ---------------------------------------------------------
    // XP boost persistence
    // ---------------------------------------------------------

    uint64 GetBoostExpiration(Player* player)
    {
        if (!player)
            return 0;

        QueryResult result = CharacterDatabase.Query(
            "SELECT `expires_at` "
            "FROM `veilborn_xp_boost` "
            "WHERE `guid` = {}",
            player->GetGUID().GetCounter()
        );

        if (!result)
            return 0;

        return (*result)[0].Get<uint64>();
    }

    bool IsXpBoostActive(Player* player)
    {
        uint64 expiresAt = GetBoostExpiration(player);

        if (!expiresAt)
            return false;

        uint64 now = static_cast<uint64>(
            GameTime::GetGameTime().count()
        );

        if (expiresAt <= now)
        {
            CharacterDatabase.DirectExecute(
                "DELETE FROM `veilborn_xp_boost` "
                "WHERE `guid` = {}",
                player->GetGUID().GetCounter()
            );

            return false;
        }

        return true;
    }

    // ---------------------------------------------------------
    // Player script
    // ---------------------------------------------------------

    class VeilbornStarterPackPlayerScript : public PlayerScript
    {
    public:
        VeilbornStarterPackPlayerScript()
            : PlayerScript("VeilbornStarterPackPlayerScript")
        {
        }

        // -----------------------------------------------------
        // First login
        // -----------------------------------------------------

        void OnPlayerFirstLogin(Player* player) override
        {
            if (!IsEnabled() || !player)
                return;

            ItemPosCountVec dest;

            uint8 result = player->CanStoreNewItem(
                NULL_BAG,
                NULL_SLOT,
                dest,
                STARTER_PACK_ITEM,
                1
            );

            if (result != EQUIP_ERR_OK)
            {
                // Inventory is full. Mail is intentionally not used
                // in this first version.
                ChatHandler(player->GetSession()).SendNotification(
                    "Your Veilborn Starter Pack could not be placed in your inventory. "
                    "Please make room in your bags."
                );

                return;
            }

            Item* item = player->StoreNewItem(
                dest,
                STARTER_PACK_ITEM,
                true
            );

            if (!item)
            {
                ChatHandler(player->GetSession()).SendNotification(
                    "Failed to create your Veilborn Starter Pack."
                );

                return;
            }

            player->SendNewItem(item, 1, true, false);

            ChatHandler(player->GetSession()).SendNotification(
                "You received a Veilborn Starter Pack. "
                "It can be opened at level %u.",
                GetMinLevel()
            );
        }

        // -----------------------------------------------------
        // Restore XP boost after login
        // -----------------------------------------------------

        void OnPlayerLogin(Player* player) override
        {
            if (!IsEnabled() || !player)
                return;

            if (!IsXpBoostActive(player))
                return;

            uint64 expiresAt = GetBoostExpiration(player);
            uint64 now = static_cast<uint64>(
                GameTime::GetGameTime().count()
            );

            uint64 remaining = expiresAt > now
                ? expiresAt - now
                : 0;

            uint32 remainingMinutes =
                static_cast<uint32>((remaining + 59) / 60);

            ChatHandler(player->GetSession()).SendNotification(
                "Your 2x experience effect is active for %u more minute(s).",
                remainingMinutes
            );
        }

        // -----------------------------------------------------
        // Prevent starter pack from opening before level 10
        // -----------------------------------------------------

        bool OnPlayerBeforeOpenItem(Player* player, Item* item) override
        {
            if (!IsEnabled() || !player || !item)
                return true;

            if (item->GetEntry() != STARTER_PACK_ITEM)
                return true;

            if (player->GetLevel() < GetMinLevel())
            {
                ChatHandler(player->GetSession()).SendNotification(
                    "You must reach level %u before opening the Veilborn Starter Pack.",
                    GetMinLevel()
                );

                return false;
            }

            return true;
        }

        // -----------------------------------------------------
        // Put gold into the loot window
        // -----------------------------------------------------

        void OnPlayerBeforeSendLoot(
            Player* player,
            ObjectGuid lootGuid,
            Loot* loot
        ) override
        {
            if (!IsEnabled() || !player || !loot)
                return;

            if (!lootGuid.IsItem())
                return;

            Item* item = player->GetItemByGuid(lootGuid);

            if (!item)
                return;

            if (item->GetEntry() != STARTER_PACK_ITEM)
                return;

            // Gold is represented in copper.
            // 100 gold = 100 * 10,000 copper.
            loot->gold = GetGold() * GOLD;
        }

        // -----------------------------------------------------
        // Double XP while potion effect is active
        // -----------------------------------------------------

        void OnPlayerGiveXP(
            Player* player,
            uint32& amount,
            Unit* /*victim*/,
            uint8 /*xpSource*/
        ) override
        {
            if (!IsEnabled() || !player || !amount)
                return;

            if (!IsXpBoostActive(player))
                return;

            uint32 multiplier = GetXpMultiplier();

            uint64 boostedAmount =
                static_cast<uint64>(amount) * multiplier;

            if (boostedAmount >
                std::numeric_limits<uint32>::max())
            {
                amount = std::numeric_limits<uint32>::max();
            }
            else
            {
                amount = static_cast<uint32>(boostedAmount);
            }
        }
    };

    // ---------------------------------------------------------
    // Potion item script
    // ---------------------------------------------------------

    class VeilbornXpPotionScript : public ItemScript
    {
    public:
        VeilbornXpPotionScript()
            : ItemScript("veilborn_xp_potion")
        {
        }

        bool OnUse(
            Player* player,
            Item* item,
            SpellCastTargets const& /*targets*/
        ) override
        {
            if (!IsEnabled() || !player || !item)
                return true;

            if (item->GetEntry() != XP_POTION_ITEM)
                return false;

            uint64 now = static_cast<uint64>(
                GameTime::GetGameTime().count()
            );

            uint64 currentExpiration =
                GetBoostExpiration(player);

            uint64 baseTime =
                std::max(now, currentExpiration);

            uint64 newExpiration =
                baseTime + GetPotionDuration();

            CharacterDatabase.DirectExecute(
                "INSERT INTO `veilborn_xp_boost` "
                "(`guid`, `expires_at`) "
                "VALUES ({}, {}) "
                "ON DUPLICATE KEY UPDATE "
                "`expires_at` = VALUES(`expires_at`)",
                player->GetGUID().GetCounter(),
                newExpiration
            );

            // Consume exactly one potion.
            player->DestroyItemCount(item, 1, true);

            uint64 remaining =
                newExpiration - now;

            uint32 remainingMinutes =
                static_cast<uint32>((remaining + 59) / 60);

            ChatHandler(player->GetSession()).SendNotification(
                "Veilborn Training Draught activated. "
                "Experience x%u for %u minute(s).",
                GetXpMultiplier(),
                remainingMinutes
            );

            return true;
        }
    };
}

void AddSC_mod_veilborn_starter_pack()
{
    using namespace VeilbornStarterPack;

    new VeilbornStarterPackPlayerScript();
    new VeilbornXpPotionScript();
}