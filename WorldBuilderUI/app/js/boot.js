// ── Init ──────────────────────────────────────────────────────────────────────
// Moved from renderer.js — must load LAST: definitions live in earlier modules.
resize();
initObjectsPanel();
initWaypointsPanel();
initTerrainPanel();
initTexturePanel();
initViewPopover();
initObjPropsPanel();
initNaturePanel();
initLightingPanel();
initSideListPanel();
initSetupPanel();
initSetupModal();
initMapSetupPanel();
initScriptsModal();
initStartScreen();
// Start with the Map Setup panel visible
switchPanel('mapsetup');
document.querySelector('[data-tool="mapsetup"]').classList.add('active');
// Opening screen: New Map / Load Map (lands on the Map Setup tab)
showStartScreen();
