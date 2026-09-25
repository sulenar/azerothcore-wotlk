-- ============================================================
-- Veilborn Starter Pack
-- ============================================================

SET @STARTER_PACK := 900001;
SET @XP_POTION := 900002;

-- ------------------------------------------------------------
-- Cleanup
-- ------------------------------------------------------------

DELETE FROM `item_loot_template`
WHERE `Entry` = @STARTER_PACK;

DELETE FROM `item_template`
WHERE `entry` IN (@STARTER_PACK, @XP_POTION);

-- ------------------------------------------------------------
-- Starter Pack
--
-- Clone Tiny Titanium Lockbox as a visual/template base.
-- ------------------------------------------------------------

INSERT INTO `item_template`
(
    `entry`,
    `class`,
    `subclass`,
    `SoundOverrideSubclass`,
    `name`,
    `displayid`,
    `Quality`,
    `Flags`,
    `FlagsExtra`,
    `BuyCount`,
    `BuyPrice`,
    `SellPrice`,
    `InventoryType`,
    `AllowableClass`,
    `AllowableRace`,
    `ItemLevel`,
    `RequiredLevel`,
    `RequiredSkill`,
    `RequiredSkillRank`,
    `requiredspell`,
    `requiredhonorrank`,
    `RequiredCityRank`,
    `RequiredReputationFaction`,
    `RequiredReputationRank`,
    `maxcount`,
    `stackable`,
    `ContainerSlots`,
    `bonding`,
    `ScriptName`
)
VALUES
(
    @STARTER_PACK,
    15,
    0,
    0,
    'Veilborn Starter Pack',
    54724,
    3,
    4,
    0,
    1,
    0,
    0,
    0,
    -1,
    -1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    ''
);

-- ------------------------------------------------------------
-- XP Potion
--
-- Clone Major Healing Potion as a visual/template base.
-- The actual effect is implemented by the module.
-- ------------------------------------------------------------

CREATE TEMPORARY TABLE `vb_xp_potion`
LIKE `item_template`;

INSERT INTO `vb_xp_potion`
SELECT *
FROM `item_template`
WHERE `entry` = 13446;

UPDATE `vb_xp_potion`
SET
    `entry` = @XP_POTION,
    `name` = 'Veilborn Training Draught',
    `description` = 'Increases experience gained by 100% for 1 hour.',
    `RequiredLevel` = 1,
    `RequiredSkill` = 0,
    `RequiredSkillRank` = 0,
    `spellid_1` = 0,
    `spelltrigger_1` = 0,
    `spellcharges_1` = 0,
    `spellcooldown_1` = 0,
    `spellcategory_1` = 0,
    `spellcategorycooldown_1` = 0,
    `stackable` = 20,
    `maxcount` = 0,
    `bonding` = 1,
    `SellPrice` = 0,
    `BuyPrice` = 0,
    `ScriptName` = 'veilborn_xp_potion';

INSERT INTO `item_template`
SELECT *
FROM `vb_xp_potion`;

DROP TEMPORARY TABLE `vb_xp_potion`;

-- ------------------------------------------------------------
-- Starter Pack loot
--
-- 5 x XP potion
-- Gold is added by C++ to the loot object.
-- ------------------------------------------------------------

INSERT INTO `item_loot_template`
(
    `Entry`,
    `Item`,
    `Reference`,
    `Chance`,
    `QuestRequired`,
    `LootMode`,
    `GroupId`,
    `MinCount`,
    `MaxCount`,
    `Comment`
)
VALUES
(
    @STARTER_PACK,
    @XP_POTION,
    0,
    100,
    0,
    1,
    0,
    5,
    5,
    'Veilborn Starter Pack: 5 XP potions'
);