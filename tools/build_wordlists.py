#!/usr/bin/env python3
"""Build data/<lang>_solutions.txt and data/<lang>_valid.txt from external
word lists. Run manually when you want to refresh the lists; the outputs are
committed so a normal build needs no network access.

Sources (downloaded into a cache directory):
  EN  original Wordle answer list + allowed guesses (cfreshman gists)
      OpenSubtitles word frequencies (hermitdave/FrequencyWords, CC-BY-SA)
  FR  Lexique 3.83 (lexique.org, CC BY-SA 4.0): forms, lemmas, POS, frequency
      an-array-of-french-words (MIT) for extra valid forms

Solutions: the N most frequent "common" words (EN: Wordle answers ranked by
subtitle frequency; FR: Lexique lemmas that are nouns/adjectives/verbs/adverbs
ranked by film+book lemma frequency), minus a small block list.
Valid words: every 5-letter a-z form (accents stripped) from all sources.

usage: build_wordlists.py [--cache DIR] [--count 500]
"""
import argparse
import csv
import io
import json
import os
import re
import sys
import unicodedata
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "data")

URLS = {
    "wordle-answers.txt": "https://gist.githubusercontent.com/cfreshman/a03ef2cba789d8cf00c08f767e0fad7b/raw/wordle-answers-alphabetical.txt",
    "wordle-guesses.txt": "https://gist.githubusercontent.com/cfreshman/cdcdf777450c5b5301e439061d29694c/raw/wordle-allowed-guesses.txt",
    "en_50k.txt": "https://raw.githubusercontent.com/hermitdave/FrequencyWords/master/content/2018/en/en_50k.txt",
    "Lexique383.zip": "http://www.lexique.org/databases/Lexique383/Lexique383.zip",
    "french-words.json": "https://raw.githubusercontent.com/words/an-array-of-french-words/master/index.json",
}

# Words we do not want as solutions (slurs, vulgarity, proper nouns that
# slipped through). They stay accepted as guesses.
BLOCK_EN = set()
BLOCK_FR = set("""
merde negre negro boche garce sucer pisse chier foutu foutre putes salop
jesus jenny harry bougre conne cocue enfoire nazie nazis pute bite
""".split())

FIVE = re.compile(r"^[a-z]{5}$")


def strip_accents(s):
    return "".join(c for c in unicodedata.normalize("NFD", s) if unicodedata.category(c) != "Mn")


def norm(word):
    w = strip_accents(word.strip().lower()).replace("œ", "oe").replace("æ", "ae")
    return w if FIVE.match(w) else None


def fetch(cache, name):
    path = os.path.join(cache, name)
    if not os.path.exists(path):
        print("downloading", URLS[name])
        os.makedirs(cache, exist_ok=True)
        with urllib.request.urlopen(URLS[name]) as r, open(path, "wb") as f:
            f.write(r.read())
    return path


def read_lines(path):
    with open(path, encoding="utf-8") as f:
        return [l.strip() for l in f if l.strip()]


def build_en(cache, count):
    answers = [w for w in (norm(x) for x in read_lines(fetch(cache, "wordle-answers.txt"))) if w]
    guesses = [w for w in (norm(x) for x in read_lines(fetch(cache, "wordle-guesses.txt"))) if w]
    freq = {}
    for line in read_lines(fetch(cache, "en_50k.txt")):
        w, n = line.split()
        w = norm(w)
        if w and w not in freq:
            freq[w] = int(n)
    answer_set = set(answers)
    ranked = sorted((w for w in answer_set if w not in BLOCK_EN), key=lambda w: -freq.get(w, 0))
    solutions = ranked[:count]
    valid = sorted(answer_set | set(guesses))
    return solutions, valid


def build_fr(cache, count):
    zpath = fetch(cache, "Lexique383.zip")
    with zipfile.ZipFile(zpath) as z:
        tsv = io.TextIOWrapper(z.open("Lexique383.tsv"), encoding="utf-8")
        reader = csv.DictReader(tsv, delimiter="\t")
        valid = set()
        lemma_freq = {}
        for row in reader:
            w = norm(row["ortho"])
            if not w:
                continue
            valid.add(w)
            if (row["islem"] == "1" and row["ortho"] == row["lemme"]
                    and row["cgram"] in ("NOM", "ADJ", "VER", "ADV")):
                f = float(row["freqlemfilms2"]) + float(row["freqlemlivres"])
                lemma_freq[w] = max(lemma_freq.get(w, 0.0), f)
    with open(fetch(cache, "french-words.json"), encoding="utf-8") as f:
        for x in json.load(f):
            w = norm(x)
            if w:
                valid.add(w)
    ranked = sorted((w for w in lemma_freq if w not in BLOCK_FR), key=lambda w: -lemma_freq[w])
    solutions = ranked[:count]
    return solutions, sorted(valid)


def write(lang, solutions, valid):
    os.makedirs(DATA, exist_ok=True)
    assert len(set(solutions)) == len(solutions)
    assert set(solutions) <= set(valid)
    with open(os.path.join(DATA, f"{lang}_solutions.txt"), "w", newline="\n") as f:
        f.write("\n".join(solutions) + "\n")
    with open(os.path.join(DATA, f"{lang}_valid.txt"), "w", newline="\n") as f:
        f.write("\n".join(valid) + "\n")
    print(f"{lang}: {len(solutions)} solutions, {len(valid)} valid words")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--cache", default=os.path.join(HERE, "..", "build", "wordlist-cache"))
    ap.add_argument("--count", type=int, default=500, help="number of solution words per language")
    a = ap.parse_args()
    write("en", *build_en(a.cache, a.count))
    write("fr", *build_fr(a.cache, a.count))
    return 0


if __name__ == "__main__":
    sys.exit(main())
