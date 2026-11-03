#!/usr/bin/env python3
"""On-device preparation with cache invalidation and atomic success publication."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
from tlou_bionic import prepare, require_game

REVISION = 1


def fingerprint(game):
    require_game(game)
    digest = hashlib.sha256()
    digest.update(f"{REVISION}:{game.resolve()}".encode())
    paths = [game / "eboot.bin", game / "sce_sys/param.sfo"]
    paths += sorted((game / "sce_module").rglob("*.prx"))
    for path in paths:
        digest.update(str(path.relative_to(game)).encode())
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    return digest.hexdigest()


def ensure(source, game, out):
    game, out = game.resolve(), out.resolve()
    if out == game or out.is_relative_to(game) or game.is_relative_to(out):
        raise ValueError("Preparation directory must be separate from the game")
    key = fingerprint(game)
    marker = out / "device-ready.json"
    if marker.is_file():
        try:
            current = json.loads(marker.read_text())
            if (current.get("fingerprint") == key and
                    (out / "boot-linked.bin").is_file() and
                    (out / "content.bin").is_file() and (out / "tlou-view").is_dir()):
                print("Jogo já preparado; arquivos verificados.", flush=True)
                return out
        except (ValueError, OSError):
            pass
    out.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".prepare-", dir=out.parent))
    backup = out.with_name(out.name + ".previous")
    try:
        print("Preparando executável, módulos e conteúdo do TLOU…", flush=True)
        prepare(source, game, None, stage)
        (stage / "device-ready.json").write_text(json.dumps({"fingerprint": key, "game": str(game)}))
        if backup.exists():
            shutil.rmtree(backup)
        if out.exists():
            out.rename(backup)
        try:
            stage.rename(out)
        except OSError:
            if backup.exists():
                backup.rename(out)
            raise
        if backup.exists():
            shutil.rmtree(backup)
        print("Preparação concluída. Pronto para jogar.", flush=True)
        return out
    finally:
        if stage.exists():
            shutil.rmtree(stage)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    ensure(Path(__file__).resolve().parents[1], args.game, args.out)
