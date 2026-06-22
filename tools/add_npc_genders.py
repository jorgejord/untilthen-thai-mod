# -*- coding: utf-8 -*-
"""Merge NPC/role/system speakers into char_gender.json (from authoritative gender report).
Skips existing entries. male->ครับ, female->ค่ะ, neutral-> no forced ending."""
import json, pathlib
P = pathlib.Path(r"C:\Users\theze\Desktop\UntilThenModeThailanguse\tools\char_gender.json")
d = json.loads(P.read_text(encoding="utf-8"))

MALE = ["Emcee","NpcEmceeMale","CoupleMan","CoupleA","MalePassenger","NpcCrinklesGuy","PantryGuy",
 "ManongCig","ManongChick","TruckDriver","TricycleDriver","KuyaGrab","NpcFlorante","NpcWaiter",
 "BasketballNPC","Soldier","NPCSoldier","NPC_Male","StudentMale","Jose","JakeDad","DesDad","NicoleDad",
 "Mem1","Mem3","Mem4","App1","NpcAnnoyingSon","Khyle","Ian","Edward","Charles","Brian","Carlo","Andrew",
 "Ronel","Lucas","Dan","Justin","Armando","Mike","PreteenMark","CutsceneMark"]
FEMALE = ["NpcEmceeFemale","CoupleWoman","CoupleB","GossipGirl1","GossipGirl2","NpcCounterGirl","BpopWoman",
 "NpcNurse","NpcMother","NpcMom","NpcLaura","MaamGreenlife","Granny","NPC_Granny","Teacher","TeacherFemale1",
 "StudentFemale","AteTherese","NicoleMom","Mem2","App2","App3","Chesca","Karen","Maura","Eva","Jessica",
 "Elise","CutsceneCathy","CathyPhone","YoungSofia","PreteenNicole","WalkwayNicole"]
# system / unknown / generic -> neutral (post_process will NOT force a gendered ending)
NEUTRAL = ["SYS","TV","PSA","Megaphone","MCL","Anon","Anonymous","Unknown","NpcGeneric","NpcEndorser",
 "BingoHost","Investigator","AttyMendoza","Driver","Caretaker","Parent","Presenter","Person1","Person2",
 "NPC1","NPC2","NPC3","NPC4","StudentA","StudentTaker","BusyStudent","NpcApp1","NpcTicket","Jogger",
 "StaffA","StaffLobby","NpcSpectator1","NpcSpectator2","Beggar1","CupsPerson","Shopkeep","Local",
 "KidD1","KidD2","KidE1","KidE2","KidF1","KidF2","NpcStudentA","NpcStudentB","NpcStudentC","NpcStudentD",
 "NpcVocalist"]

def add(names, gender):
    pron = {"male":"ผม/เรา","female":"ฉัน/เรา","neutral":"เรา"}[gender]
    end  = {"male":"ครับ","female":"ค่ะ","neutral":""}[gender]
    n=0
    for nm in names:
        if nm in d: continue
        d[nm] = {"gender":gender,"pronoun":pron,"ending":end,"role":"NPC/role","src":"gender-report"}
        n+=1
    return n

added = add(MALE,"male")+add(FEMALE,"female")+add(NEUTRAL,"neutral")
P.write_text(json.dumps(d, ensure_ascii=False, indent=2), encoding="utf-8")
real = [k for k in d if not k.startswith("_")]
print(f"added {added} new NPC/role/system entries. total now {len(real)} characters (+meta).")
print("ambiguous still unconfirmed:", d.get("_ambiguous_need_confirm"))
