extends SceneTree

func _init():
	var raw = FileAccess.get_file_as_string("res://combined_th.json")
	var data = JSON.parse_string(raw)
	if data == null:
		print("ERROR: parse")
		quit()
		return
	var t = Translation.new()
	t.locale = "fil"   # hijack Filipino slot for the rough Thai register
	var n = 0
	for k in data.keys():
		t.add_message(k, data[k])
		n += 1
	var err = ResourceSaver.save(t, "res://text.fil.translation")
	print("SAVED fil keys=", n, " err=", err)
	var loaded = load("res://text.fil.translation")
	print("VERIFY locale=", loaded.locale, " MAIN_MENU_START len=", str(loaded.get_message("MAIN_MENU_START")).length())
	quit()
