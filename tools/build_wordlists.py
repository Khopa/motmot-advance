#!/usr/bin/env python3
"""Build data/<lang>_solutions.txt and data/<lang>_valid.txt from external
word lists. Run manually when you want to refresh the lists; the outputs are
committed so a normal build needs no network access.

Sources (downloaded into a cache directory):
  EN  an-array-of-english-words (MIT) as the dictionary
      OpenSubtitles word frequencies (hermitdave/FrequencyWords, CC-BY-SA)
      google-10000-english (Google Trillion Word corpus, "no swears" list)
      random-name first names list, used to exclude first names
  FR  Lexique 3.83 (lexique.org, CC BY-SA 4.0): forms, lemmas, POS, frequency
      an-array-of-french-words (MIT) for extra valid forms
  ES  Letterpress word list (lorenbrichter/Words, CC0) + OpenSubtitles frequencies
  DE  Letterpress word list (CC0) + OpenSubtitles frequencies; umlauts and
      sharp s are written ae/oe/ue/ss (the keyboard is A-Z only)
  IT  paroleitaliane (napolux, MIT) + OpenSubtitles frequencies

Solutions: the N most frequent "common" words (EN: dictionary words present
in both the subtitle and the web top lists, ranked by subtitle frequency,
first names removed; FR: Lexique lemmas that are nouns/adjectives/verbs/
adverbs ranked by film+book lemma frequency; ES/DE/IT: dictionary words
ranked by subtitle frequency, first names removed), minus a small block
list per language. Valid words: every 5-letter a-z form (accents stripped)
known to the dictionaries (with at least a few subtitle occurrences for the
languages without a curated lexicon).

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
    "english-words.json": "https://raw.githubusercontent.com/words/an-array-of-english-words/master/index.json",
    "en_full.txt": "https://raw.githubusercontent.com/hermitdave/FrequencyWords/master/content/2018/en/en_full.txt",
    "google-10000.txt": "https://raw.githubusercontent.com/first20hours/google-10000-english/master/google-10000-english-no-swears.txt",
    "first-names.txt": "https://raw.githubusercontent.com/dominictarr/random-name/master/first-names.txt",
    "Lexique383.zip": "http://www.lexique.org/databases/Lexique383/Lexique383.zip",
    "french-words.json": "https://raw.githubusercontent.com/words/an-array-of-french-words/master/index.json",
    "words_es.txt": "https://raw.githubusercontent.com/lorenbrichter/Words/master/Words/es.txt",
    "words_de.txt": "https://raw.githubusercontent.com/lorenbrichter/Words/master/Words/de.txt",
    "words_it.txt": "https://raw.githubusercontent.com/napolux/paroleitaliane/master/paroleitaliane/660000_parole_italiane.txt",
    "es_full.txt": "https://raw.githubusercontent.com/hermitdave/FrequencyWords/master/content/2018/es/es_full.txt",
    "de_full.txt": "https://raw.githubusercontent.com/hermitdave/FrequencyWords/master/content/2018/de/de_full.txt",
    "it_full.txt": "https://raw.githubusercontent.com/hermitdave/FrequencyWords/master/content/2018/it/it_full.txt",
}

# Words we do not want as solutions (slurs, vulgarity, proper nouns and
# subtitle contractions that slipped through). They stay accepted as guesses.
BLOCK_EN = set("""
jesus peter james henry harry jimmy roger smith chuck louis jones mason lewis
tyler oscar ralph paris china india japan vegas texas spain
gonna wanna gotta haven kinda lemme dunno
""".split())
BLOCK_FR = set("""
merde negre negro boche garce sucer pisse chier foutu foutre putes salop
jesus jenny harry bougre conne cocue enfoire nazie nazis pute bite
""".split())
BLOCK_ES = set("""
joder perra zorra puta putas mierda james henry paris simon julio china india
texas jesus
""".split())
BLOCK_DE = set("""
arsch peter david harry james henry jimmy jesus clark jason steve chuck roger
louis kevin scott simon brian larry jones klaus lucas texas bruce paris china
juden senor daddy sorry story madam drink nazis
""".split())
BLOCK_IT = set("""
cazzo merda sesso troia porca tette porno henry oscar bruce coach drink
james jesus paris texas
""".split())

# minimum subtitle occurrences for a dictionary word to be accepted (EN/ES/DE/IT)
EN_MIN_FREQ = 3

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
    with open(fetch(cache, "english-words.json"), encoding="utf-8") as f:
        dictionary = {w for w in (norm(x) for x in json.load(f)) if w}
    freq = {}
    for line in read_lines(fetch(cache, "en_full.txt")):
        w, n = line.split()
        w = norm(w)
        if w and w not in freq:
            freq[w] = int(n)
    web_top = {w for w in (norm(x) for x in read_lines(fetch(cache, "google-10000.txt"))) if w}
    with open(fetch(cache, "first-names.txt"), encoding="utf-8", errors="ignore") as f:
        names = {w for w in (norm(x) for x in f) if w}

    valid = {w for w in dictionary if freq.get(w, 0) >= EN_MIN_FREQ}
    candidates = (w for w in valid if w in web_top and w not in names and w not in BLOCK_EN)
    solutions = sorted(candidates, key=lambda w: -freq[w])[:count]
    return solutions, sorted(valid)


# German has no umlauts on an A-Z keyboard: the usual ASCII spellings
def norm_de(word):
    w = word.strip().lower().replace("\u00e4", "ae").replace("\u00f6", "oe").replace("\u00fc", "ue").replace("\u00df", "ss")
    return norm(w)


def build_from_dictionary(cache, count, dict_file, freq_file, block, normalise=norm):
    """Generic builder: plain dictionary + full subtitle frequency list."""
    with open(fetch(cache, dict_file), encoding="utf-8", errors="ignore") as f:
        dictionary = {w for w in (normalise(x) for x in f) if w}
    freq = {}
    for line in read_lines(fetch(cache, freq_file)):
        w, n = line.split()
        w = normalise(w)
        if w and w not in freq:
            freq[w] = int(n)
    with open(fetch(cache, "first-names.txt"), encoding="utf-8", errors="ignore") as f:
        names = {w for w in (norm(x) for x in f) if w}

    valid = {w for w in dictionary if freq.get(w, 0) >= EN_MIN_FREQ}
    candidates = (w for w in valid if w not in names and w not in block)
    solutions = sorted(candidates, key=lambda w: -freq[w])[:count]
    return solutions, sorted(valid)


def build_es(cache, count):
    return build_from_dictionary(cache, count, "words_es.txt", "es_full.txt", BLOCK_ES)


def build_de(cache, count):
    return build_from_dictionary(cache, count, "words_de.txt", "de_full.txt", BLOCK_DE, norm_de)


def build_it(cache, count):
    return build_from_dictionary(cache, count, "words_it.txt", "it_full.txt", BLOCK_IT)


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
    write("es", *build_es(a.cache, a.count))
    write("de", *build_de(a.cache, a.count))
    write("it", *build_it(a.cache, a.count))
    return 0


if __name__ == "__main__":
    sys.exit(main())
