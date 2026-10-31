#!/usr/bin/env python3
"""Grammar metadata and lexical-pattern checks, not a TextMate engine test."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GRAMMAR = json.loads((ROOT / 'editors/vscode/syntaxes/nextviper.tmLanguage.json').read_text())


class GrammarContract(unittest.TestCase):
    def test_metadata_matches_extension_manifest(self):
        manifest = json.loads((ROOT / 'editors/vscode/package.json').read_text())
        self.assertEqual(GRAMMAR['scopeName'], 'source.nextviper')
        self.assertIn('nv', GRAMMAR['fileTypes'])
        language = next(item for item in manifest['contributes']['languages'] if item['id'] == 'nextviper')
        self.assertIn('.nv', language['extensions'])

    def test_keyword_patterns_match_lexer(self):
        source = (ROOT / 'src/lexer.cpp').read_text()
        keywords = set(re.findall(r'\{"(\w+)", TokenType::KEYWORD_', source))
        patterns = [re.compile(pattern['match']) for pattern in GRAMMAR['patterns']
                    if pattern.get('name') in ('keyword.control.nextviper', 'keyword.declaration.nextviper',
                                               'constant.language.nextviper')]
        for word in keywords:
            self.assertTrue(any(pattern.fullmatch(word) for pattern in patterns), word)
        for word in ('try', 'catch', 'finally', 'throw', 'class', 'interface', 'enum', 'const', 'none', 'letter'):
            self.assertFalse(any(pattern.fullmatch(word) for pattern in patterns), word)

    def test_compound_operator_patterns_consume_full_token(self):
        pattern = re.compile(next(item['match'] for item in GRAMMAR['patterns']
                                  if item.get('name') == 'keyword.operator.nextviper'))
        for operator in ('+=', '-=', '*=', '/=', '%=', '**', '=>', '->', '..', '..='):
            self.assertEqual(pattern.match(operator).group(), operator)


if __name__ == '__main__':
    unittest.main(verbosity=2)
