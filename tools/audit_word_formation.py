"""Check every shipped lexical headword and export a teacher-readable decision list."""
import argparse
import csv
import json
import re
from collections import defaultdict
from pathlib import Path

from build_word_formation import DATA, INCLUDE, ROOT, build, component, read_table, teaching_head


def inventory(library, reserve, csv_decks):
    sources = defaultdict(set)
    def add(source, word):
        sources[source].add(word.strip().lower())

    for line in (DATA / "catalog.ndjson").read_bytes().splitlines():
        if line.strip():
            row = json.loads(line)
            add(row["stage"], row["word"])
    native = (ROOT / "firmware/components/native_learning/generated/learning_data.cpp").read_text("utf8")
    chunks = re.findall(r'R"NATIVE_DATA\((.*?)\)NATIVE_DATA"', native, re.S)
    for row in json.loads("".join(chunks))["catalog"]:
        add("native-course", row["word"])

    packs = {}
    for path in sorted(library.glob("*/*/manifest.json")):
        manifest = json.loads(path.read_bytes())
        key = manifest["id"] + "@" + manifest["version"]
        packs[key] = manifest, json.loads(path.with_name("content.json").read_bytes())
    if not packs:
        raise ValueError(f"No installed packs in {library}")
    for key, (manifest, pack) in packs.items():
        for row in pack.get("words", {}).values():
            add(key, row["word"])
        for term in pack.get("terms", {}):
            add(key, term)
        for chapter in pack.get("chapters", []):
            for row in chapter.get("words", []):
                if isinstance(row, dict) and "term" in row:
                    add(key, row["term"])
                if isinstance(row, dict) and "id" in row:
                    ref = packs[manifest["requires"]][1]["words"][row["id"]]
                    add(key, ref["word"])
    for directory in csv_decks:
        paths = sorted(directory.glob("*.csv"))
        if not paths:
            raise ValueError(f"No CSV decks in {directory}")
        for path in paths:
            with path.open(encoding="utf-8-sig", newline="") as stream:
                for row in csv.DictReader(stream):
                    if "sentence" not in row.get("tags", "").lower().split("|"):
                        add("CSV/" + path.stem, row["front"])
    active = set.union(*sources.values())
    if reserve:
        for row in json.loads(reserve.read_bytes()):
            for word in [row["word"], *row["alternatives"]]:
                add("inventory-" + row["stage"], word)
        for row in json.loads(reserve.with_name("supplementary.json").read_bytes()):
            add("supplementary", row["word"])
    return sources, active


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--library", type=Path, required=True)
    ap.add_argument("--inventory", type=Path)
    ap.add_argument("--decks", type=Path, action="append", default=[], help="A directory of vocabulary card CSVs")
    ap.add_argument("--csv", type=Path)
    args = ap.parse_args()
    choices = read_table(DATA / "word-formation.tsv")
    meanings = read_table(DATA / "word-parts.tsv")
    if build(choices, meanings) != INCLUDE.read_text("utf8"):
        raise ValueError("Stale compiled index")
    sources, active = inventory(args.library, args.inventory, args.decks)
    all_words = set.union(*sources.values())
    missing = all_words - choices.keys()
    if missing:
        raise ValueError(f"No editorial decision for {sorted(missing)}")
    analyzed = {w for w, parts in choices.items() if parts != "="}
    compound = {w for w in analyzed if sum(component(k)[1] == "Base" for k in choices[teaching_head(w, choices)].split("|")) >= 2}
    if args.csv:
        with args.csv.open("w", encoding="utf-8-sig", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["词形或原始卡面", "来源", "处理", "所拆词形", "构词式", "逐项释义"])
            for w in sorted(choices):
                groups = [s for s, entries in sources.items() if w in entries]
                kind = "保留整词"
                parts = []
                if w in analyzed:
                    kind = "复合构词" if w in compound else "派生或词根分析"
                    parts = choices[teaching_head(w, choices)].split("|")
                labels = {"Base": "组成词" if w in compound else "词基", "Root": "词根", "Prefix": "前缀", "Suffix": "后缀"}
                columns = [component(k) for k in parts]
                details = [f"{labels[role]} {form}（{meanings[k]}）" for k, (form, role) in zip(parts, columns)]
                writer.writerow([w, " / ".join(groups) or "补充词族", kind, teaching_head(w, choices) if parts else "", " + ".join(form for form, _ in columns), "；".join(details)])
    def counts(words):
        return {"spellings": len(words), "analyses": len(words & analyzed), "compounds": len(words & compound),
                "grouped_labels": sum(choices[w].startswith(">") for w in words), "whole": len(words - analyzed)}
    print(json.dumps({"active": counts(active), "all_sources": counts(all_words),
                      "index": counts(set(choices)), "part_meanings": len(meanings),
                      "sources": {s: counts(v) for s, v in sources.items()}}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
