CREATE TABLE IF NOT EXISTS `veilborn_xp_boost` (
    `guid` INT UNSIGNED NOT NULL,
    `expires_at` BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
