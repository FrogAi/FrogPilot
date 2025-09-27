#!/usr/bin/env python3

import argparse
import json
import os
import pathlib
import xml.etree.ElementTree as ET
from concurrent.futures import ThreadPoolExecutor, as_completed
from requests.adapters import HTTPAdapter
from typing import cast
from urllib3.util.retry import Retry

import requests

TRANSLATIONS_DIR = pathlib.Path(__file__).resolve().parent
TRANSLATIONS_LANGUAGES = TRANSLATIONS_DIR / "languages.json"

OPENAI_MODEL = "gpt-5"
OPENAI_API_KEY = os.environ.get("OPENAI_API_KEY")

FUN_LANG_KEYS = {"caveman", "duck", "frog", "pirate", "shakespearean"}

FUN_PROMPT_TEMPLATE = """
You are a playful *style translator* for openpilot. Translate the following message (an English source string) into the style '{language}'. Output ONLY the translated text, with no quotes or extra words.

Output rules:
- Input: one English UI string from a Qt .ts file.
- Style key: {language}.
- Output: a fun, stylized rewrite that keeps technical structure intact.

Hard requirements:
1) Preserve placeholders, variables, and markup exactly as written: {{name}}, {{0}}, {{icu}}, %1, %n, %(speed)d, $SPEED, <b>…</b>, <a href="…">…</a>, etc.
2) Keep all non-translatable tokens unchanged: product/brand names (e.g., openpilot, ACC), file paths, error codes, part numbers.
3) Do not add, remove, or reorder placeholders. If grammar absolutely requires reordering, keep all placeholders intact and still produce a correct sentence; prefer wordings that avoid reordering.
4) Do not convert units or numbers (e.g., mph↔km/h). Translate unit labels only if standard in the style and not part of a preserved token.
5) Maintain the same warning/priority level and imperative tone. Never soften or intensify safety messages ("Do not…", "Warning", "Critical").
6) Preserve hotkeys/accelerators if present (e.g., &F, _O). If the exact letter is impossible, pick the nearest mnemonic but keep the marker.
7) Follow style punctuation and casing norms while respecting all technical tokens.
8) If ICU MessageFormat/plural/select syntax is present, keep the structure and variable names unchanged and rewrite only the human-readable text.
9) Keep the output as concise as the source. Do not append notes, explanations, or metadata.

Style Hints:
- caveman: Short, blunt sentences. Simple words. Little grammar. Example: "Me want food. You come now."
- duck: Quacky interjections, waddling rhythm, silly tone. Example: "Quack! What you mean? Waddle-waddle, quack!"
- frog: Croaky, ribbit-filled speech, jumpy tone. Example: "Ribbit! I hop to help you. Croak, ribbit!"
- pirate: Rough, nautical slang, dropped consonants, lots of "Arr!" Example: "Arr, ye scallywag! Hoist the sails 'n fetch me rum!"
- shakespearean: Flowery, old-fashioned English, thee/thou, dramatic flair. Example: "Prithee, good sir, thou dost jest most cruelly!"

Keep length close to source; avoid bloat. Respond with the styled string only.
"""

OPENAI_PROMPT = """
You are a safety-critical UI translator for openpilot. Translate the following message (an English source string) into the locale '{language}'. Output ONLY the translated text, with no quotes or extra words.

Hard requirements:
1) Preserve placeholders, variables, and markup exactly as written: {{name}}, {{0}}, {{icu}}, %1, %n, %(speed)d, $SPEED, <b>…</b>, <a href="…">…</a>, etc.
2) Keep all non-translatable tokens unchanged: product/brand names (e.g., openpilot, ACC), file paths, error codes, part numbers.
3) Do not add, remove, or reorder placeholders. If grammar absolutely requires reordering, keep all placeholders intact and still produce a correct sentence; prefer wordings that avoid reordering.
4) Do not convert units or numbers (e.g., mph↔km/h). Translate unit labels only if standard in the target locale and not part of a preserved token.
5) Maintain the same warning/priority level and imperative tone. Never soften or intensify safety messages ("Do not…", "Warning", "Critical").
6) Preserve hotkeys/accelerators if present (e.g., &F, _O). If the exact letter is impossible, pick the nearest mnemonic but keep the marker.
7) Follow target-locale punctuation and casing norms while respecting all technical tokens.
8) If ICU MessageFormat/plural/select syntax is present, keep the structure and variable names unchanged and translate only the human-readable text.
9) Keep the translation as concise as the source. Do not append notes, explanations, or metadata.

If the source is ambiguous or untranslatable without more context, choose the safest literal rendering that preserves meaning. If you cannot translate without risking meaning loss, return the source text unchanged.

Your entire reply must be a single line containing only the final translation.
"""

OPENAI_EVAL_PROMPT = """
You are a safety-critical reviewer for UI translations for openpilot. Your job is to compare two candidate translations (A and B) of an English source string and select the safest, most accurate option in the locale '{language}'.

Output rules:
- Return ONLY one line containing exactly one of these: the full text of Translation A, or the full text of Translation B, or the exact Source string.
- Do not include quotes, labels, explanations, or whitespace beyond the chosen text.

Decision criteria (apply in order):
1) Hard correctness checks vs the Source:
   - All placeholders/variables/markup from the Source must be preserved verbatim and remain valid: {{name}}, {{0}}, {{icu}}, %1, %n, %(speed)d, $SPEED, <b>…</b>, <a href="…">…</a>, &amp;, etc.
   - Numbers and units must match; no unit conversion (mph↔km/h) and no number changes.
   - Non-translatable tokens present in Source must remain unchanged (e.g., openpilot, ACC, file paths, error codes, part numbers).
   - Hotkeys/accelerators such as &F or _O must be preserved with a sensible mnemonic letter in the target language; the marker must remain.
   - ICU MessageFormat/plural/select syntax must keep the same structure and variable names; translate only human-readable text.
   If only one candidate passes all hard checks, select it. If both fail, return the exact Source string.

2) Meaning, tone, and severity:
   - Preserve the precise meaning and intent; do not add, omit, soften, or intensify warnings/errors/imperatives.
   - Keep safety language direct and unambiguous.
   If only one candidate preserves meaning/tone precisely, select it.

3) Target-language quality:
   - Must be written in the target locale '{language}', idiomatic, grammatically correct, and concise while retaining meaning.
   - Follow locale-appropriate punctuation, spacing, and capitalization, without altering required technical tokens.
   Prefer the candidate that best satisfies these.

4) Tie-breakers (when both are acceptable and equally accurate):
   - Prefer the one closer in length to the Source and more readable on small UI.
   - Prefer consistent terminology with common automotive/HMI usage in the target locale.
   - Prefer minimal reordering of placeholders if both are valid.

Remember:
- Never fabricate or "improve" content. Choose A or B, or fall back to the Source if both are unsafe.
- Your reply must be exactly the chosen string with no commentary.
"""

SESSION = requests.Session()

def configure_session():
  if OPENAI_API_KEY:
    SESSION.headers.update({
      "Authorization": f"Bearer {OPENAI_API_KEY}",
      "Content-Type": "application/json"
    })
  retry = Retry(
    total=10,
    backoff_factor=1,
    status_forcelist=[429, 500, 502, 503, 504],
    allowed_methods=frozenset(["POST"])
  )
  adapter = HTTPAdapter(pool_connections=100, pool_maxsize=100, max_retries=retry)
  SESSION.mount("https://", adapter)
  SESSION.mount("http://", adapter)


def get_language_files(languages: list[str] = None) -> dict[str, pathlib.Path]:
  files = {}

  with open(TRANSLATIONS_LANGUAGES) as fp:
    language_dict = json.load(fp)

    for filename in language_dict.values():
      path = TRANSLATIONS_DIR / f"{filename}.ts"
      language = path.stem.split("main_")[1]

      if languages is None or language in languages:
        files[language] = path

  return files


def evaluate_translation(source: str, old: str, new: str, language: str) -> str:
  try:
    response = SESSION.post(
      "https://api.openai.com/v1/chat/completions",
      json={
        "model": OPENAI_MODEL,
        "messages": [
          {"role": "system", "content": OPENAI_EVAL_PROMPT.format(language=language)},
          {"role": "user", "content": f"Source: {source}\n\nTranslation A: {old}\n\nTranslation B: {new}"},
        ],
        "max_completion_tokens": 2048,
        "reasoning_effort": "medium",
        "verbosity": "low",
      },
      timeout=(10, 60)
    )

    if 400 <= response.status_code < 600:
      raise requests.HTTPError(f'Error {response.status_code}: {response.text}', response=response)

    data = response.json()
    return cast(str, data["choices"][0]["message"]["content"])
  except Exception as e:
    print(f"Evaluation failed for '{source[:40]}...': {e}")
    return old


def translate_phrase(text: str, language: str) -> str:
  lang_key = language.strip().lower()
  if lang_key in FUN_LANG_KEYS:
    prompt = FUN_PROMPT_TEMPLATE.format(language=lang_key)
  else:
    prompt = OPENAI_PROMPT.format(language=language)

  try:
    response = SESSION.post(
      "https://api.openai.com/v1/chat/completions",
      json={
        "model": OPENAI_MODEL,
        "messages": [
          {"role": "system", "content": prompt},
          {"role": "user", "content": text},
        ],
        "max_completion_tokens": 2048,
        "reasoning_effort": "minimal",
        "verbosity": "low",
      },
      timeout=(10, 60)
    )

    if 400 <= response.status_code < 600:
      print(f'Error {response.status_code}: {response.text}')
      return ""

    data = response.json()
    return cast(str, data["choices"][0]["message"]["content"])
  except Exception as e:
    print(f"Translation failed for '{text[:40]}...': {e}")
    return ""


def translate_file(path: pathlib.Path, language: str, all_: bool, vet_translations: bool) -> None:
  tree = ET.parse(path)
  root = tree.getroot()

  for context in root.findall("./context"):
    name = context.find("name")
    if name is None:
      raise ValueError("name not found")

    print(f"Context: {name.text}")

    work_items = []

    for message in context.findall("./message"):
      source = message.find("source")
      translation = message.find("translation")

      if source is None or translation is None:
        raise ValueError("source or translation not found")

      translation_type = translation.attrib.get("type", "")

      if vet_translations:
        if "-generated" not in translation_type:
          continue
      elif not all_:
        if translation_type != "unfinished":
          if translation_type.endswith("-generated") and not translation_type.startswith(OPENAI_MODEL):
            pass
          else:
            continue

      text = cast(str, source.text)
      numerus = (message.attrib.get("numerus") == "yes") or ("%n" in text)
      old_translation = translation.text or ""

      work_items.append((message, translation, text, numerus, old_translation))

    if not work_items:
      continue

    def worker(item):
      message, translation, text, numerus, old_translation = item
      llm_translation = translate_phrase(text, language)

      if vet_translations:
        best = evaluate_translation(text, old_translation, llm_translation, language)
        return (message, translation, text, numerus, best, True)
      else:
        return (message, translation, text, numerus, llm_translation, False)

    results = []
    with ThreadPoolExecutor(max_workers=100) as executor:
      future_map = {executor.submit(worker, item): item for item in work_items}
      for future in as_completed(future_map):
        try:
          results.append(future.result())
        except Exception as e:
          item = future_map[future]
          print(f"Task failed for '{item[2][:40]}...': {e}")

    for message, translation, text, numerus, chosen_translation, was_vetted in results:
      print(f"Source: {text}\nCurrent translation: {translation.text}\nLLM translation: {chosen_translation}")

      if was_vetted:
        print(f"Chosen translation: {chosen_translation}")
        translation.text = chosen_translation
      else:
        translation.set("type", f"{OPENAI_MODEL}-generated")

        if numerus:
          translations = chosen_translation or (translation.text or text)
          for child in list(translation):
            translation.remove(child)

          translation.text = None

          ET.SubElement(translation, "numerusform").text = translations
          ET.SubElement(translation, "numerusform").text = translations
        else:
          translation.text = chosen_translation

  with path.open("w", encoding="utf-8") as fp:
    fp.write('<?xml version="1.0" encoding="utf-8"?>\n' +
             '<!DOCTYPE TS>\n' +
             ET.tostring(root, encoding="utf-8").decode())


def main():
  arg_parser = argparse.ArgumentParser("Auto translate")

  group = arg_parser.add_mutually_exclusive_group(required=True)
  group.add_argument("-a", "--all-files", action="store_true", help="Translate all files")
  group.add_argument("-f", "--file", nargs="+", help="Translate the selected files. (Example: -f fr de)")

  arg_parser.add_argument("-t", "--all-translations", action="store_true", default=False, help="Translate all sections. (Default: only unfinished)")
  arg_parser.add_argument("-v", "--vet-translations", action="store_true", default=False, help="Re-evaluate AI-generated translations")

  args = arg_parser.parse_args()

  if OPENAI_API_KEY is None:
    print("OpenAI API key is missing. (Hint: use `export OPENAI_API_KEY=YOUR-KEY` before you run the script).\n" +
          "If you don't have one go to: https://beta.openai.com/account/api-keys.")
    exit(1)

  configure_session()

  files = get_language_files(None if args.all_files else args.file)

  if args.file:
    missing_files = set(args.file) - set(files)
    if len(missing_files):
      print(f"No language files found: {missing_files}")
      exit(1)

  if args.vet_translations:
    print(f"Re-evaluating all translations with the '{OPENAI_MODEL}-generated' type.")
  else:
    print(f"Translation mode: {'all' if args.all_translations else 'only unfinished'}. Files: {list(files)}")

  for lang, path in files.items():
    print(f"Translate {lang} ({path})")
    translate_file(path, lang, args.all_translations, args.vet_translations)


if __name__ == "__main__":
  main()
