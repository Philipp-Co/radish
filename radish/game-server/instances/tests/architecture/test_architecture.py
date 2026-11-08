"""
Architekturregeln des game-servers als Tests (PyTestArch, wie ArchUnit fuer
Java): welche Pakete welche importieren duerfen.

Schichten: models <- domain (Spiele auf den Instanzen aufsetzen und abbrechen)
<- interfaces (die Endpunkte fuer das Backend). Nach unten importieren ist
erlaubt, nach oben nicht.
"""

from pathlib import Path

from django.test import SimpleTestCase
from pytestarch import EvaluableArchitecture, Rule, get_evaluable_architecture

# Das Paket instances/ ist zugleich die Wurzel: so heissen die Module wie in
# den Importen ("instances.domain..."), nicht nach dem Verzeichnis darueber.
INSTANCES_DIR: Path = Path(__file__).resolve().parents[2]


class ArchitectureTests(SimpleTestCase):
    evaluable: EvaluableArchitecture

    @classmethod
    def setUpClass(cls) -> None:
        super().setUpClass()
        cls.evaluable = get_evaluable_architecture(
            str(INSTANCES_DIR), str(INSTANCES_DIR), exclusions=("*migrations*",)
        )

    def assert_does_not_import(self, importer: str, importees: list[str]) -> None:
        """Kein Modul unter importer importiert eines unter importees."""
        (
            Rule()
            .modules_that().are_sub_modules_of(importer)
            .should_not().import_modules_that().are_sub_modules_of(importees)
            .assert_applies(self.evaluable)
        )

    def test_models_import_neither_domain_nor_interfaces(self) -> None:
        self.assert_does_not_import("instances.models", ["instances.domain", "instances.interfaces"])

    def test_domain_does_not_import_interfaces(self) -> None:
        self.assert_does_not_import("instances.domain", ["instances.interfaces"])
