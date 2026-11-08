"""Check the game package's public option contract against its host defaults."""
import pathlib
import sys
import tomllib

manifest = tomllib.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
assert manifest["id"] == "moh.input.mouse-keyboard"
assert manifest["target"] == [{"game_id": "SLUS-00974"}]
assert len(manifest["feature"]) == 1
assert manifest["feature"][0]["id"] == "mouse-keyboard"
assert manifest["feature"][0]["default_enabled"] is False
assert manifest["plugin"] == [{"feature": "mouse-keyboard", "id": manifest["id"]}]
options = {option["id"]: option for option in manifest["option"]}
assert len(options) == len(manifest["option"])
for name, default in (("look_sensitivity", 5), ("aim_sensitivity", 4)):
    option = options[name]
    assert (option["type"], option["min"], option["max"], option["step"],
            option["slider"], option["default"]) == ("integer", 1, 10, 1, True, default)
zoom = options["aimed_zoom"]
assert (zoom["type"], zoom["min"], zoom["max"], zoom["step"], zoom["slider"],
        zoom["display_divisor"], zoom["default"]) == ("integer", 100, 250, 1, True, 100, 125)
defaults = {
    "forward": "key:26", "backward": "key:22", "left": "key:4", "right": "key:7",
    "fire": "mouse:1,key:40", "aim": "mouse:3", "reload": "key:21", "use": "key:8",
    "jump": "key:44,key:42", "crouch": "key:224", "next_weapon": "key:20,wheel:down",
    "prev_weapon": "wheel:up", "pause": "key:19", "objectives": "key:43",
    "menu_up": "key:26,key:82", "menu_down": "key:22,key:81", "menu_left": "key:4,key:80",
    "menu_right": "key:7,key:79", "menu_confirm": "key:8,key:40,mouse:1",
    "menu_back": "key:20,key:42",
}
assert set(options) == {"look_sensitivity", "aim_sensitivity", "aimed_zoom"} | {
    "bind_" + action for action in defaults
}
for action, default in defaults.items():
    option = options["bind_" + action]
    assert option["feature"] == "mouse-keyboard" and option["type"] == "binding"
    assert option["default"] == default and len(default.split(",")) <= 3
    assert option.get("allow_wheel", False) == (action in ("next_weapon", "prev_weapon"))
