# utils/helpers.py
import re
import urllib.parse
import logging
from typing import Optional, Dict
from config import SKIN_API_URL

logger = logging.getLogger("TeeworldsBridge.Utils")

COUNTRY_TO_ISO_MAP: Dict[int, Optional[str]] = {
    -1:  None,
    # custom
    905: "EU",
    # ISO 3166-2 subdivisions (no default :flag_xx:)
    901: "GB-ENG",  # England -> нет :flag_gb_eng:
    902: "GB-NIR",  # Northern Ireland
    903: "GB-SCT",  # Scotland
    904: "GB-WLS",  # Wales
    906: "ES-CT",   # Catalonia
    907: "ES-GA",   # Galicia
    # ISO 3166-1 based (can has :flag_xx:)
    4:   "AF",
    248: "AX",
    8:   "AL",
    12:  "DZ",
    16:  "AS",
    20:  "AD",
    24:  "AO",
    660: "AI",
    10:  "AQ",
    28:  "AG",
    32:  "AR",
    51:  "AM",
    533: "AW",
    36:  "AU",
    40:  "AT",
    31:  "AZ",
    44:  "BS",
    48:  "BH",
    50:  "BD",
    52:  "BB",
    112: "BY",
    56:  "BE",
    84:  "BZ",
    204: "BJ",
    60:  "BM",
    64:  "BT",
    68:  "BO",
    70:  "BA",
    72:  "BW",
    76:  "BR",
    86:  "IO",
    96:  "BN",
    100: "BG",
    854: "BF",
    108: "BI",
    116: "KH", # Cambodia - KH, но в оригинальной таблице CV = 132, KH = 116. Проверяем! (CV=132, KH=116 - верно)
    120: "CM",
    124: "CA",
    132: "CV", # Cape Verde
    136: "KY",
    140: "CF",
    148: "TD",
    152: "CL",
    156: "CN",
    162: "CX",
    166: "CC",
    170: "CO",
    174: "KM",
    178: "CG", # Congo
    180: "CD", # Congo DR
    184: "CK",
    188: "CR",
    384: "CI", # Cote d'Ivoire
    191: "HR",
    192: "CU",
    531: "CW",
    196: "CY",
    203: "CZ",
    208: "DK",
    262: "DJ",
    212: "DM",
    214: "DO",
    218: "EC",
    818: "EG",
    222: "SV",
    226: "GQ",
    232: "ER",
    233: "EE",
    231: "ET",
    238: "FK",
    234: "FO",
    242: "FJ",
    246: "FI",
    250: "FR",
    254: "GF",
    258: "PF",
    260: "TF", # French Southern Territories (нет эмодзи)
    266: "GA",
    270: "GM",
    268: "GE",
    276: "DE",
    288: "GH",
    292: "GI",
    300: "GR",
    304: "GL",
    308: "GD",
    312: "GP",
    316: "GU",
    320: "GT",
    831: "GG", # Guernsey
    324: "GN",
    624: "GW",
    328: "GY",
    332: "HT",
    336: "VA",
    340: "HN",
    344: "HK",
    348: "HU",
    352: "IS",
    356: "IN",
    360: "ID",
    364: "IR",
    368: "IQ",
    372: "IE",
    833: "IM", # Isle of Man
    376: "IL",
    380: "IT",
    388: "JM",
    392: "JP",
    832: "JE", # Jersey
    400: "JO",
    398: "KZ",
    404: "KE",
    296: "KI",
    408: "KP", # North Korea
    410: "KR", # South Korea
    414: "KW",
    417: "KG",
    418: "LA",
    428: "LV",
    422: "LB",
    426: "LS",
    430: "LR",
    434: "LY",
    438: "LI",
    440: "LT",
    442: "LU",
    446: "MO",
    807: "MK", # North Macedonia
    450: "MG",
    454: "MW",
    458: "MY",
    462: "MV",
    466: "ML",
    470: "MT",
    584: "MH",
    474: "MQ",
    478: "MR",
    480: "MU",
    484: "MX",
    583: "FM", # Micronesia
    498: "MD",
    492: "MC",
    496: "MN",
    499: "ME",
    500: "MS",
    504: "MA",
    508: "MZ",
    104: "MM",
    516: "NA",
    520: "NR",
    524: "NP",
    528: "NL",
    540: "NC",
    554: "NZ",
    558: "NI",
    562: "NE",
    566: "NG",
    570: "NU",
    574: "NF",
    580: "MP",
    578: "NO",
    512: "OM",
    586: "PK",
    585: "PW",
    275: "PS", # Palestine
    591: "PA",
    598: "PG",
    600: "PY",
    604: "PE",
    608: "PH",
    612: "PN",
    616: "PL",
    620: "PT",
    630: "PR",
    634: "QA",
    638: "RE", # Réunion
    642: "RO",
    643: "RU",
    646: "RW",
    652: "BL",
    654: "SH",
    659: "KN",
    662: "LC",
    663: "MF",
    666: "PM",
    670: "VC",
    882: "WS",
    674: "SM",
    678: "ST",
    682: "SA",
    686: "SN",
    688: "RS",
    690: "SC",
    694: "SL",
    702: "SG",
    534: "SX",
    703: "SK",
    705: "SI",
    90:  "SB",
    706: "SO",
    710: "ZA",
    239: "GS",
    728: "SS", # South Sudan
    724: "ES",
    144: "LK",
    729: "SD", # Sudan
    740: "SR",
    748: "SZ", # Eswatini (бывш. Swaziland)
    752: "SE",
    756: "CH",
    760: "SY",
    158: "TW", # Taiwan
    762: "TJ",
    834: "TZ",
    764: "TH",
    626: "TL",
    768: "TG",
    772: "TK",
    776: "TO",
    780: "TT",
    788: "TN",
    792: "TR",
    795: "TM",
    796: "TC",
    798: "TV",
    800: "UG",
    804: "UA",
    784: "AE",
    826: "GB", # United Kingdom
    840: "US",
    858: "UY",
    860: "UZ",
    548: "VU",
    862: "VE",
    704: "VN",
    92:  "VG",
    850: "VI",
    876: "WF",
    732: "EH", # Western Sahara
    887: "YE",
    894: "ZM",
    716: "ZW",
}

def get_flag_emoji(country_code: int) -> str:
    """Converts a Teeworlds country code to a Discord flag emoji string."""
    iso_code = COUNTRY_TO_ISO_MAP.get(country_code)
    unknown_emoji_flag = "🌐"

    if iso_code:
        if len(iso_code) == 2:
            try:
                regional_indicators = "".join([chr(0x1F1E6 + ord(char.upper()) - ord('A')) for char in iso_code])
                logger.debug(f"Converted country code {country_code} (ISO: {iso_code}) to emoji: {regional_indicators}")
                return regional_indicators
            except Exception as e:
                logger.warning(f"Could not convert ISO code '{iso_code}' to regional indicators: {e}")
                return unknown_emoji_flag

        elif iso_code == "EU":
            logger.debug(f"Using specific emoji for EU (code {country_code})")
            return "🇪🇺"
        else:
            logger.debug(f"No standard :flag_xx: emoji for code {country_code} (Mapped to: {iso_code}). Using fallback.")
            return unknown_emoji_flag
    else:
        logger.debug(f"Country code {country_code} not found in map. Using fallback emoji.")
        return unknown_emoji_flag


def normalize_nickname(nickname: str) -> str:
    """Removes common clan tags or prefixes from nicknames."""
    normalized = re.sub(r"^[\[|{(].*?[\]|})]\s*", "", nickname).strip()
    return normalized


def get_skin_url(skin_name: str, use_custom_color: bool, body_color: int, feet_color: int) -> str:
    """Generates a URL to render a Teeworlds skin via skins_api."""
    if not skin_name:
        skin_name = "default"

    base_url = (SKIN_API_URL or "http://127.0.0.1:8000/render").rstrip("/")
    params = {"name": skin_name}
    if use_custom_color:
        params["body"] = str(body_color)
        params["foot"] = str(feet_color)

    query = urllib.parse.urlencode(params, quote_via=urllib.parse.quote)
    skin_api_url = f"{base_url}?{query}"
    logger.debug(f"Generated skin URL: {skin_api_url}")
    return skin_api_url


def clean_message(text: str) -> str:
    """Removes non-printable characters except common whitespace."""
    cleaned = text.replace('\x00', '')
    return ''.join(ch for ch in cleaned if ch.isprintable() or ch in '\n\r\t').strip()
