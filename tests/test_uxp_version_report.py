import json
from pathlib import Path
import re
import unittest


class UxpVersionReportTests(unittest.TestCase):
    def test_loaded_heartbeat_and_response_report_the_manifest_version(self):
        root=Path(__file__).resolve().parents[1]/'uxp/offline-probe'
        version=json.loads((root/'manifest.json').read_text('utf-8'))['version']
        source=(root/'main.js').read_text('utf-8')
        declared=re.search(r"const probeVersion = '([^']+)'",source)
        self.assertIsNotNone(declared)
        self.assertEqual(declared[1],version)
        self.assertRegex(source,r"probe_version:\s*probeVersion, session, transport")
        self.assertRegex(source,r"const response = \{schema:1,session,id: request.id, probe_version:\s*probeVersion")
