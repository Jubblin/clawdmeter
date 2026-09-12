"""PlatformIO test runner for the plain-`main()` host tests in this folder.

The tests have no framework: they print `FAIL <file>:<line>  <expr>` for every
failed check and a `... checks passed` line when all of them hold, so this
runner just maps those two shapes onto PlatformIO test cases.
"""

import re

import click
from platformio.test.result import TestCase, TestCaseSource, TestStatus
from platformio.test.runners.base import TestRunnerBase

FAIL_RE = re.compile(r"^FAIL\s+(?P<file>[^:]+):(?P<line>\d+)\s+(?P<message>.+)$")


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        if self.options.verbose:
            click.echo(line, nl=False)
        text = line.strip()
        failure = FAIL_RE.match(text)
        if failure:
            self.test_suite.add_case(
                TestCase(
                    name=failure.group("message"),
                    status=TestStatus.FAILED,
                    message=text,
                    source=TestCaseSource(
                        filename=failure.group("file"),
                        line=int(failure.group("line")),
                    ),
                )
            )
        elif text.endswith("checks passed"):
            self.test_suite.add_case(
                TestCase(
                    name=self.test_suite.test_name,
                    status=TestStatus.PASSED,
                    message=text,
                )
            )
