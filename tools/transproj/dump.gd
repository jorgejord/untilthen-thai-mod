extends SceneTree
func _init():
    var t = load("res://text.en.translation")
    var f = FileAccess.open("res://cand_keys.txt", FileAccess.READ)
    var real = []
    while not f.eof_reached():
        var k = f.get_line().strip_edges()
        if k == "": continue
        var v = t.get_message(k)
        if v != null and str(v) != "" and str(v) != k:
            real.append(k + "\t" + str(v).replace("\n","\n"))
    f.close()
    var o = FileAccess.open("res://en_real.tsv", FileAccess.WRITE)
    o.store_string("\n".join(real))
    o.close()
    print("REAL_KEYS ", real.size())
    quit()
