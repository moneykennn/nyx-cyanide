"""Generate a RepoTweaks catalog only for a supplied real HTTPS hosting URL."""
import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import urlsplit

parser = argparse.ArgumentParser()
parser.add_argument("base_url", help="HTTPS directory hosting overlay-probe.js")
parser.add_argument("--experimental-esp", action="store_true", help="Generate entry for the unverified custom Cyanide fork")
args = parser.parse_args()
url = urlsplit(args.base_url)
if url.scheme != "https" or not url.hostname or url.username or url.password or url.query or url.fragment:
    parser.error("Provide an HTTPS directory URL without credentials, query, or fragment")
root = Path(__file__).resolve().parent
script = root / ("esp-experimental.js" if args.experimental_esp else "overlay-probe.js")
version = hashlib.sha256(script.read_bytes()).hexdigest()[:12]
is_esp = args.experimental_esp
catalog = {
    "repoName": "Nyx Cyanide Prototypes",
    "tweaks": [{
        "id": "nyx.overlay.esp-experimental" if is_esp else "nyx.overlay.display-probe",
        "name": "Nyx ESP (experimental)" if is_esp else "Nyx Display Test (not ESP)",
        "version": "0.1.0-" + version,
        "description": ("Requires the unverified custom Cyanide-Nyx fork; will not work on stock Cyanide. No anti-ban guarantee."
                        if is_esp else "Unverified display-only prototype. No PUBG data. Remove using the Remove test window setting and Apply Tweaks."),
        "scriptURL": args.base_url.rstrip("/") + "/" + script.name,
        "symbol": "rectangle.on.rectangle",
        "minIOS": "17.5.1",
        "maxIOS": "17.5.1",
        "compatibilityNote": "Target: iPhone SE 3 on iOS 17.5.1. Device behavior has not been verified."
    }]
}
output = root / "repotweaks.json"
output.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(output)
