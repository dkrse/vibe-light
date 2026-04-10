# Prompt Features — Plán

## Kontext

Na základe skúseností s ~30 projektmi rôzneho typu (web, utility, GUI app):
- Projekty sa vyvíjajú max týždeň intenzívne, potom len drobnosti
- Prompty sa prakticky nikdy neopakujú
- Výnimka: 2 fixné akcie po každej zmene
- Efektívnejšie je pracovať po malých krokoch a vidieť výsledok
- Keď AI nevie niečo vyriešiť, nesnažiť sa to riešiť v tom istom cykle — reštartovať model a začať nanovo
- Po každej zmene aktualizovať docs (architecture.md, changelog.md, diagrams.md s mermaid, readme.md) — dokumentácia opisuje systém v aktuálnom stave, vrátane vyriešených bugov a ako sa riešili
- Po docs update vždy commit + push — záchytný bod pre konkrétnu verziu (aj keď reálne sa návrat k staršej verzii zatiaľ nikdy nepotreboval)
- Niekedy jednoducho vyzerajúce úlohy sa zasekávajú — detailný záznam riešenia bugov (čo sa skúšalo, čo nefungovalo, čo nakoniec pomohlo) môže ušetriť čas ak sa podobný problém objaví znova
- Pridať `docs/bugs.md` — rozšírený popis bugov oproti stručnému changelog, vrátane postupu riešenia
- Pri systémových aplikáciách (C/C++, GTK, ...) AI by mal po každej zmene sám urobiť rebuild, skontrolovať warnings/errors a vyriešiť ich bez zásahu — šetrí to výrazne čas
- Opačná skúsenosť s Docker — zastavovanie/reštartovanie serverov nechať na človeka, AI v tom robí zmätky a zaberie to viac času aj keď sa to nakoniec podarí
- AI niekedy pomení kód a nedokáže ho vrátiť do pôvodného stavu — preto časté commity po každej fungujúcej zmene, aby sa dal urobiť git revert/checkout
- Commit message = verzia z changelog.md — jednoducho sa orientuje čo kde je
- changelog.md je najdôležitejší doc — možno by mal byť detailnejší (paradoxne, lebo changelog býva stručný)

### Skúsenosti mimo kód (dokumenty, články)
- Konverzia .md → .tex je problematická, lepšie písať rovno v .tex aj keď treba kompilovať
- AI má problémy s väčšími tabuľkami v LaTeX (prekračujú okraj)
- Grafické prvky v .tex sú takmer nemožné — lepšie je urobiť diagram cez mermaid, exportovať ako obrázok a vložiť

### Filozofia workflow
- AI integrácia v editoroch (Zed, VS Code, Copilot) kde AI ukazuje zmeny a čaká na súhlas/rejection je strata času
- Kód sa nečíta — číta sa len dokumentácia a diagramy
- AI má mať voľnú ruku robiť zmeny v kóde, človek kontroluje výsledok cez docs a funkčnosť, nie cez review kódu
- Preto terminálový prístup (Claude Code) je efektívnejší než IDE integrácie — žiadne čakanie na approval jednotlivých zmien
- Žiadny manuálny zásah do kódu — ani premenovanie tlačidla. Všetko cez prompt, aj triviálne zmeny. Konzistentný workflow bez výnimiek.
- Dokumentácia je teraz najdôležitejšia — presný opak oproti tradičnému vývoju. Predtým bola dokumentácia povinnosť, teraz je to primárne rozhranie medzi vývojárom a produktom. Kód číta a píše AI, človek číta dokumentáciu.

## Čo NIE JE potrebné

- **Prompt história / navigácia** — nikdy nebola potreba opakovať prompt
- **Saved prompts / bookmarky** — zbytočné
- **Terminal output capture** — nikdy sa nečíta spätne
- **Prompt search / tagging** — over-engineering
- **Session management** — nepotrebné pri krátkych projektoch
- **Prompt log/audit** — nice-to-have ale reálne sa nečíta

## Čo implementovať

### Quick Actions (konfigurovateľné tlačidlá)

Dve fixné akcie ktoré sa reálne opakujú po každej zmene:

1. **Analyze** — odošle prompt na analýzu rýchlosti, pamäťového managementu a bezpečnosti
2. **Update docs** — odošle prompt na aktualizáciu docs/architecture.md, docs/changelog.md, docs/diagrams.md, readme.md

**Implementácia:**
- Tlačidlá v prompt paneli (alebo klávesové skratky)
- Text akcií konfigurovateľný v settings alebo v projektovom súbore (`.LLM/actions.json` alebo podobne)
- Rôzne projekty môžu mať rôzne akcie

## Ako to robia ostatní

Dominantný vzor v ekosystéme AI coding nástrojov: **markdown súbory v repozitári, vyvolané ako slash príkazy**.

| Nástroj | Persistentný kontext | Znovupoužiteľné príkazy |
|---------|---------------------|-------------------------|
| Claude Code | `CLAUDE.md` | `.claude/commands/*.md` → `/commandname` |
| Cursor | `.cursor/rules/*.mdc` | Rules s "manual" invoke mode |
| Continue.dev | `continue.config.json` | `.prompt` súbory → slash commands |
| Aider | Convention files | Komunitný aider-script |
| Cline | `.clinerules` | `prompts/` adresár (komunitný vzor) |

**Kľúčový pattern:** Markdown súbory s prompt textom, uložené v projekte, vyvolané jedným kliknutím/príkazom. Čo je v podstate to isté ako Quick Actions — len uložené ako súbory namiesto JSON konfigurácie.

### Zvážiť

Quick Actions by mohli byť `.md` súbory v adresári projektu (napr. `.vibe/actions/analyze.md`). Výhody:
- Verzionovateľné cez git
- Editovateľné v ľubovoľnom editore
- Zdieľateľné medzi tímom
- Kompatibilné s existujúcim ekosystémovým vzorom
