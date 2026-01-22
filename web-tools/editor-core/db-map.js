(() => {
  // Central DB map used by all editors.
  // Define semantic keys once and reuse everywhere.
  //
  // Each entry:
  // - source: whitelisted source name for api/db.php
  // - labelMode: how options are displayed
  //     - 'id_name' => "ID: Name"
  //     - 'name'    => "Name"

  const DB_MAP = {
    quest: { source: 'quests', labelMode: 'id_name' },
    item:  { source: 'items',  labelMode: 'id_name' },
    mob:   { source: 'bots',   labelMode: 'id_name' },
    world: { source: 'worlds', labelMode: 'id_name' },
    skill: { source: 'skills', labelMode: 'id_name' },
  };

  window.EditorCore = window.EditorCore || {};
  window.EditorCore.DBMap = DB_MAP;
})();
