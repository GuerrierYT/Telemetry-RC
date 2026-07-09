# Checklist de creation du depot GitHub

## Avant le premier commit

- Verifier que `include/config.local.h` existe uniquement en local et n'est pas suivi par Git.
- Compiler avec `pio run`.
- Televerser LittleFS au moins une fois avec `pio run -t uploadfs` sur une carte de test.
- Relire `README.md` et ajuster le materiel cible si necessaire.
- Choisir une licence avant publication publique.

## Initialiser le depot local

Si le dossier n'est pas encore un depot Git valide :

```powershell
git init -b main
git add .
git commit -m "Initial project import"
```

## Creer le depot distant

Creer un depot vide sur GitHub, sans README, sans `.gitignore` et sans licence automatique, puis lier le remote :

```powershell
git remote add origin git@github.com:VOTRE_COMPTE/NOM_DU_DEPOT.git
git push -u origin main
```

## Verification rapide avant push

```powershell
git status --short
git status --ignored --short
rg -n "password|token|secret|api[_-]?key|ssid" .
```

La recherche peut trouver `include/config.example.h` et `README.md`; c'est normal. Elle ne doit pas trouver de valeur privee.
