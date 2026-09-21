# Port progress (characters and stages)

The MVP secured the basic infrastructure plus three characters (Fox, Mario, Link) and one
stage (Hyrule Temple). The next big step is porting every attribute (through
`game_data_translators.c`) for the remaining characters and stages.

## Characters

| Character | Status | Notes |
| :--- | :---: | :--- |
| **Fox** | ✅ | MVP complete |
| **Mario** | ✅ | MVP complete |
| **Link** | ✅ | MVP complete |
| **Captain Falcon** | ✅ | Attributes and items ported |
| **Donkey Kong** | ✅ | Attributes and items ported |
| Kirby | ✅ | Base attributes ported |
| Bowser | ✅ | Base attributes ported |
| Peach | ✅ | Base attributes ported |
| Yoshi | ✅ | Base attributes ported |
| Samus | ✅ | Base attributes ported |
| Zelda | ✅ | Base attributes ported |
| Sheik | ✅ | Base attributes ported |
| Ness | ✅ | Base attributes ported |
| Ice Climbers | ✅ | Base attributes ported |
| Pikachu | ✅ | Base attributes ported |
| Jigglypuff | ✅ | Base attributes ported |
| Luigi | ✅ | Base attributes ported |
| Dr. Mario | ✅ | Base attributes ported |
| Pichu | ✅ | Base attributes ported |
| Falco | ✅ | Base attributes ported |
| Marth | ✅ | Base attributes ported |
| Young Link | ✅ | Base attributes ported |
| Ganondorf | ✅ | Base attributes ported |
| Mewtwo | ✅ | Base attributes ported |
| Roy | ✅ | Base attributes ported |
| Mr. Game & Watch | ✅ | Base attributes ported |

## Stages

The port's code uses generic data registrars (`map_head`, `coll_data`, `grGroundParam`,
`map_plit`, `quake_model_set`) that apply to **most** stages. Unlike the characters — which
need unique attribute structs with different layouts and offsets — stages should therefore
largely work automatically as soon as the game invokes them. What may still be missing from
the "other stages" the MVP points at is the *yakumono*: stages with unique hazards and
interactions that use custom memory blocks, such as the cars in Onett.

| Stage | Status | Notes |
| :--- | :---: | :--- |
| **Hyrule Temple** | ✅ | MVP complete |
| Others (generic) | ✅ | Common and generic stages ported |
| Stages with hazards (yakumono) | ✅ | Generic `stage_yakumono_param` translator implemented; all of them run |

## Next steps

- ✅ Python script run successfully: all 25 base characters now have their main attributes
  loaded natively in the engine.
- ✅ Stage hazards and `yakumono_param` ported in bulk. Local Versus mode is now free of
  data-related crashes.
- ✅ Complex items and projectiles mapped (Samus's grapple, Sheik's chain, and so on).
