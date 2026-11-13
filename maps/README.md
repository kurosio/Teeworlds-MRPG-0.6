# Maps and multiworlds

Maps live under `maps/`. A world is not registered by adding its file to a JSON list: the running server loads world records from the `tw_worlds` table in the shared database (`mrpg_shared.sql`). Each record's `Path` points to a map file relative to `maps/` (for example, `worlds/Forest.map`). The `ID` is the world identifier used by other world-specific database records, so preserve it when editing or migrating world data.

To add a world, install its `.map` file at the path referenced by the database record and create/update the corresponding `tw_worlds` row with the required world metadata. Keep the record and file path in sync. Review foreign-key and `WorldID` references before changing an existing world's ID. A map file alone does not create a world or update its database configuration.

The `dungeons/`, `minigames/`, `pvp/`, `rhythm/`, and `worlds/` directories organize map assets by use; their directory names do not replace the database record's `Path`.
