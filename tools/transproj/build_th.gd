extends SceneTree

func _init():
	var raw = FileAccess.get_file_as_string("res://combined_th.json")
	var data = JSON.parse_string(raw)
	if data == null:
		print("ERROR: could not parse combined_th.json")
		quit()
		return
	var t = Translation.new()
	t.locale = "th"
	var n = 0
	for k in data.keys():
		t.add_message(k, data[k])
		n += 1
	var err = ResourceSaver.save(t, "res://text.th.translation")
	print("SAVED th  keys=", n, "  err=", err)
	# verify round-trip
	var loaded = load("res://text.th.translation")
	print("VERIFY locale=", loaded.locale)
	print("VERIFY MAIN_MENU_START=", loaded.get_message("MAIN_MENU_START"))
	print("VERIFY SETTINGS_LANGUAGE=", loaded.get_message("SETTINGS_LANGUAGE"))
	print("VERIFY PRE_OP_SHE_WAS=", loaded.get_message("PRE_OP_SHE_WAS"))
	quit()
