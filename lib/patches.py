"""Patch catalogue (patches/NN-name/patch.json) and the interactive selector."""
import json
import os

from .util import REPO, die

PATCH_DIR = os.path.join(REPO, "patches")


class Patch:
    def __init__(self, folder):
        self.id = os.path.basename(folder)
        self.dir = folder
        with open(os.path.join(folder, "patch.json")) as fh:
            d = json.load(fh)
        self.data = d
        self.name = d["name"]
        self.description = d.get("description", "")
        self.default = d.get("default", True)
        self.required = d.get("required", False)
        self.experimental = d.get("experimental", False)
        self.requires = d.get("requires", [])

    def file(self, rel):
        return os.path.join(self.dir, rel)

    def __getitem__(self, k):
        return self.data.get(k)


def load():
    out = []
    for d in sorted(os.listdir(PATCH_DIR)):
        if os.path.isfile(os.path.join(PATCH_DIR, d, "patch.json")):
            out.append(Patch(os.path.join(PATCH_DIR, d)))
    ids = {p.id for p in out}
    for p in out:
        for r in p.requires:
            if r not in ids:
                die("patch %s requires unknown patch %s" % (p.id, r))
    return out


def resolve(patches, selected):
    """Add requirements and required patches; return (ordered list, auto-added ids)."""
    by_id = {p.id: p for p in patches}
    sel = set(selected) | {p.id for p in patches if p.required}
    added = set()
    changed = True
    while changed:
        changed = False
        for pid in list(sel):
            for r in by_id[pid].requires:
                if r not in sel:
                    sel.add(r)
                    added.add(r)
                    changed = True
    return [p for p in patches if p.id in sel], added


def dependents(patches, pid):
    return [p.id for p in patches if pid in p.requires]


def _short(p):
    return p.id.split("-", 1)[1] if "-" in p.id else p.id


def match(patches, token):
    """Find a patch by full id, id without number, or number prefix."""
    for p in patches:
        if token in (p.id, _short(p), p.id.split("-", 1)[0]):
            return p
    die("unknown patch '%s' (see --list)" % token)


def menu(patches, selected):
    """Text-mode toggle list. Works in any terminal (Linux, WSL, Git Bash)."""
    sel = set(selected)
    toggleable = [p for p in patches if not p.required]
    show_help = True
    while True:
        print()
        print("  Patches to apply  (number = toggle, a = all, n = none, d = defaults,")
        print("                     i<number> = details, Enter = continue, q = quit)")
        print()
        for i, p in enumerate(patches):
            if p.required:
                mark = "[=]"
            else:
                mark = "[x]" if p.id in sel else "[ ]"
            tag = "  (EXPERIMENTAL)" if p.experimental and "EXPERIMENTAL" not in p.name else ""
            num = "  " if p.required else "%2d" % (toggleable.index(p) + 1)
            print("  %s %s %s%s" % (num, mark, p.name, tag))
        if show_help:
            print()
            print("  [=] always installed.  Patches needed by others are switched on automatically.")
            show_help = False
        try:
            ans = input("\n  > ").strip().lower()
        except EOFError:
            ans = ""
        if ans == "":
            return sorted(sel)
        if ans == "q":
            raise SystemExit(1)
        if ans == "a":
            sel = {p.id for p in toggleable}
            continue
        if ans == "n":
            sel = set()
            continue
        if ans == "d":
            sel = {p.id for p in toggleable if p.default}
            continue
        detail = ans.startswith("i")
        nums = ans[1:] if detail else ans
        for tok in nums.replace(",", " ").split():
            if not tok.isdigit() or not 1 <= int(tok) <= len(toggleable):
                print("  ? '%s'" % tok)
                continue
            p = toggleable[int(tok) - 1]
            if detail:
                print("\n  %s\n  %s" % (p.name, "-" * len(p.name)))
                print("  " + p.description)
                if p.requires:
                    print("  needs: " + ", ".join(p.requires))
                continue
            if p.id in sel:
                sel.discard(p.id)
                for dep in dependents(patches, p.id):
                    if dep in sel:
                        sel.discard(dep)
                        print("  also off: %s (needs %s)" % (dep, p.id))
            else:
                sel.add(p.id)
                for r in p.requires:
                    sel.add(r)
