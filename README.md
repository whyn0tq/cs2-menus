# cs2-menus (Game Utils)

Authors: Pisex, [whyn0tq](https://github.com/whyn0tq)

Fork of [Pisex/cs2-menus](https://github.com/Pisex/cs2-menus) updated for Metamod:Source 2.0.0.1483 (KHook instead of SourceHook) and the AlliedModders [s2sdk](https://github.com/alliedmodders/s2sdk/tree/cs2) `cs2` branch.

Requirements:
- Metamod:Source 2.0.0.1483 or newer
- [whyn0tq/SchemaEntity](https://github.com/whyn0tq/SchemaEntity) next to this repository when building

Changes in this fork:
- all SourceHook hooks ported to KHook
- builds against s2sdk `cs2`, no private SDK headers needed
- `CCSCustomHudLayout` support written from scratch in SchemaEntity
- Client lifecycle and packet hooks use `CServerSideClientBase`.
- the `ProcessClientVoiceData` hook was removed because the function no longer exists in the game

The language setting is - addons/configs/core.cfg

Signatures - addons/configs/signatures.ini

In 2.0.0, plugins that inspect network messages use `INetworkListener` and register it with `IUtilsApi::AddNetworkListener`. Its callbacks receive `NetMessageInfo_t`. Existing server listeners continue to receive server events. Network messages are sent only to registered network listeners.

Query `MenuOwnerApi001` to associate menus with a plugin ID. Call `CloseMenus` before unloading so active menus release the plugin's callbacks.
