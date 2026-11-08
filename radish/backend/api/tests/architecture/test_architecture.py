"""
Architekturregeln des Backends als Tests (PyTestArch, wie ArchUnit fuer Java):
welche Pakete welche importieren duerfen.

Schichten: models (Datenbankmodelle) <- domain (Geschaeftslogik) <- interfaces
(HTTP und WebSocket). Nach unten importieren ist erlaubt, nach oben nicht. In
interfaces/ sind die Bereiche admin, player und servers voneinander
unabhaengig; auth (Login, Token, Rollen) duerfen alle nutzen, auth selbst
nutzt keinen Bereich. Was mehrere Bereiche brauchen, gehoert nach domain/.
"""

from pathlib import Path

from django.test import SimpleTestCase
from pytestarch import EvaluableArchitecture, Rule, get_evaluable_architecture

# Das Paket api/ ist zugleich die Wurzel: so heissen die Module wie in den
# Importen ("api.domain..."), nicht nach dem Verzeichnis darueber.
API_DIR: Path = Path(__file__).resolve().parents[2]

AREAS: tuple[str, ...] = ("api.interfaces.admin", "api.interfaces.player", "api.interfaces.servers")


class ArchitectureTests(SimpleTestCase):
    evaluable: EvaluableArchitecture

    @classmethod
    def setUpClass(cls) -> None:
        super().setUpClass()
        cls.evaluable = get_evaluable_architecture(str(API_DIR), str(API_DIR), exclusions=("*migrations*",))

    def assert_does_not_import(self, importer: str, importees: list[str]) -> None:
        """Kein Modul unter importer importiert eines unter importees."""
        (
            Rule()
            .modules_that().are_sub_modules_of(importer)
            .should_not().import_modules_that().are_sub_modules_of(importees)
            .assert_applies(self.evaluable)
        )

    def test_models_import_neither_domain_nor_interfaces(self) -> None:
        self.assert_does_not_import("api.models", ["api.domain", "api.interfaces"])

    def test_domain_does_not_import_interfaces(self) -> None:
        self.assert_does_not_import("api.domain", ["api.interfaces"])

    def test_interface_areas_are_independent(self) -> None:
        area: str
        for area in AREAS:
            with self.subTest(area=area):
                self.assert_does_not_import(area, [other for other in AREAS if other != area])

    def test_auth_uses_no_area(self) -> None:
        self.assert_does_not_import("api.interfaces.auth", list(AREAS))
