"""
Die Datenbankmodelle des Backends, eine Datei je Model.

Django laedt beim Start "api.models", also dieses Paket. Damit die Modelle
dabei registriert werden, muss es jede Datei importieren -- ein neues Model
gehoert deshalb auch hier in die Liste. Von aussen wird ueber das Paket
importiert: from api.models import Game.
"""

from api.models.game_server import GameServer
from api.models.player import Player
from api.models.game_status import GameStatus
from api.models.game import Game

# ---------------------------------------------------------------------------
# Armeen
#
# Zwei Ebenen: der Katalog (Species, UnitType, EntityProfile, Weapon,
# Equipment) legt fest, was es gibt und welche Werte es hat -- gepflegt
# kuenftig ueber eigene Admin-Schnittstellen (Rollen kommen per OIDC aus
# Keycloak, deshalb bewusst NICHT ueber Djangos admin.py). Die Armee eines
# Spielers (Army, Unit, UnitEntity) waehlt nur aus diesem Katalog aus und
# traegt selbst keine Werte.
#
# Katalogeintraege sind per PROTECT gegen das Loeschen geschuetzt, solange
# eine Armee sie noch verwendet -- sonst verschwaenden beim Aufraeumen des
# Katalogs still Einheiten aus den Armeen der Spieler.
# ---------------------------------------------------------------------------

# Kosten: Entitaet (ueber ihr Profil), Waffe und Ausruestung haben je einen
# festen Preis im Katalog. Was eine Armee kostet, wird daraus berechnet
# (Army/Unit/UnitEntity.total_cost) und nicht gespeichert -- sonst liefe
# der Wert beim Aendern eines Preises im Katalog auseinander. Ohne Default:
# ein vergessener Preis soll auffallen, nicht still 0 kosten.

from api.models.limits import (
    SPRITE_DRAW_WIDTH_MAX,
    SPRITE_DURATION_MS_MAX,
    SPRITE_FILE_NAME_PATTERN,
    SPRITE_FPS_MAX,
    SPRITE_FRAME_SIZE_MAX,
    SPRITE_FRAMES_MAX,
    SPRITE_IMAGE_BYTES_MAX,
    MAX_NAME_BYTES,
    MAX_SPRITE_KEY_LENGTH,
    MAX_UNIT_ENTITIES,
    MAX_WEAPONS_PER_ENTITY,
    SPRITE_KEY_PATTERN,
    TERRAIN_DECOR_MAX,
    TERRAIN_DECOR_OFFSET_MAX,
    EFFECT_FIRST_FRAME_MAX,
    MaxUtf8BytesValidator,
)
from api.models.species import Species
from api.models.unit_type import UnitType
from api.models.entity_profile import EntityProfile
from api.models.weapon_class import WeaponClass
from api.models.weapon import Weapon
from api.models.equipment import Equipment
from api.models.terrain_code import TerrainCode
from api.models.terrain_type import TerrainType
from api.models.terrain_image import TerrainImage
from api.models.terrain_decor import TerrainDecor
from api.models.army import Army
from api.models.unit import Unit
from api.models.unit_entity import UnitEntity
from api.models.unit_entity_weapon import UnitEntityWeapon
from api.models.unit_entity_equipment import UnitEntityEquipment

# ---------------------------------------------------------------------------
# Einheitenbilder
#
# Die Bilder fuer die Einheiten im Spiel (UnitSprite mit seinen PNGs und
# Atlanten) -- alle hier; der Client-Build bringt keine mit.
# Ein Einheitentyp nennt eines ueber seinen Schluessel (UnitType.sprite).
# ---------------------------------------------------------------------------

from api.models.unit_sprite import UnitSprite
from api.models.unit_sprite_image import UnitSpriteImage
from api.models.unit_sprite_atlas import ROTATIONS, SpriteAnimation, SpriteFacing, UnitSpriteAtlas

# ---------------------------------------------------------------------------
# Effekte
#
# Die Bilder der Effekte im Spiel, etwa der Explosionen (Effect mit seinen
# PNGs und je Richtung einem Atlas) -- wie die Einheitenbilder alle hier; der
# Client-Build bringt keine mit. Kein Katalogeintrag nennt sie, der Client
# findet sie im Manifest.
# ---------------------------------------------------------------------------

from api.models.effect import Effect, EffectKind
from api.models.effect_image import EffectImage
from api.models.effect_atlas import EffectAtlas

__all__ = [
    "GameServer",
    "Player",
    "GameStatus",
    "Game",
    "MAX_NAME_BYTES",
    "MAX_SPRITE_KEY_LENGTH",
    "MAX_UNIT_ENTITIES",
    "MAX_WEAPONS_PER_ENTITY",
    "SPRITE_KEY_PATTERN",
    "MaxUtf8BytesValidator",
    "Species",
    "UnitType",
    "EntityProfile",
    "WeaponClass",
    "Weapon",
    "Equipment",
    "TerrainCode",
    "TerrainType",
    "TerrainImage",
    "TerrainDecor",
    "TERRAIN_DECOR_MAX",
    "TERRAIN_DECOR_OFFSET_MAX",
    "Army",
    "Unit",
    "UnitEntity",
    "UnitEntityWeapon",
    "UnitEntityEquipment",
    "UnitSprite",
    "UnitSpriteImage",
    "UnitSpriteAtlas",
    "SpriteAnimation",
    "SpriteFacing",
    "ROTATIONS",
    "SPRITE_FRAME_SIZE_MAX",
    "SPRITE_FRAMES_MAX",
    "SPRITE_DRAW_WIDTH_MAX",
    "SPRITE_FPS_MAX",
    "SPRITE_DURATION_MS_MAX",
    "SPRITE_FILE_NAME_PATTERN",
    "SPRITE_IMAGE_BYTES_MAX",
    "Effect",
    "EffectKind",
    "EffectImage",
    "EffectAtlas",
    "EFFECT_FIRST_FRAME_MAX",
]
