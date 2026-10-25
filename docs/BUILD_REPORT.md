# SPACE MINER — Windows release verification

Built with **Godot 4.4.1 stable**, Windows x64, compatibility renderer.

| Check | Result |
| --- | --- |
| Source runtime suite | 144 passed / 0 failed |
| Exported main-scene smoke launch on Windows | Passed |
| Exported SPACE_MINER.exe on Windows | 144 passed / 0 failed |
| Silent NSIS installation | Passed |
| Installed executable on Windows | 144 passed / 0 failed |
| Embedded resource package inspection | Passed |
| PE architecture / subsystem | PE32+ x86-64 / GUI, no console |
| Source archive integrity | Passed |

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| SPACE_MINER.exe | 101508016 | 7396D377C778C93EAECB30AEA73B9008A7A0FE31C7889420A6F8BD9DDC4CA64D |
| SPACE_MINER_SETUP.exe | 26998697 | 161C2411AE4DDA0398F31E7CDA1C21ACDE8C3D10B61968AAD83151A16C890FCE |
| SPACE_MINER_SOURCE.zip | 6010645 | 691364B7682626B7104798103CD071BC6C1F4C688EFF1697E31A7CDC07D951B4 |

Verified on GitHub Actions windows-latest: 2026-10-25 15:23:56 UTC.
