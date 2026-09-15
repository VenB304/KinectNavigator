# KinectNavigator.Gui.ps1 - windowed installer + config editor.
#
# Swaps the game's Kinect10.dll for the KinectNavigator shim (and back), and
# edits kinectnav.ini in the game folder live. Dot-sourced by KinectNavigator.ps1
# (the entry point), which has already loaded KinectNavigator.Core.psm1 and
# picked a language. Anything fatal here must surface as a MessageBox, not
# Write-Host -- see KinectNavigator.ps1's Show-FatalError.
param(
    [Parameter(Mandatory = $true)]$Paths,
    [Parameter(Mandatory = $true)]$BootConfig
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
try { [System.Windows.Forms.Application]::EnableVisualStyles() } catch { }
try { [System.Windows.Forms.Application]::SetCompatibleTextRenderingDefault($false) } catch { }

$ScriptDir  = $Paths.ScriptDir
$ShimSrc    = $Paths.ShimSrc
$ExampleIni = $Paths.ExampleIni

$script:Loading    = $false        # true while controls are being populated from the ini
$script:dlgLoading = $false        # ditto, for the "more settings" dialog
$script:CurState   = $null

# ---- embedded flag bitmaps (20x15 PNGs), same set as LegacyDownloader ----
$script:FlagB64 = @{
    'de'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAYAAADkmO9VAAABxUlEQVQ4ja3Ty24URxQA0FPVNWT8APwIEYoMgjVIZpdIUf4i4g/5APb5CUvsYY1lGxuPPY/uWyyqEZOJ2SBaurpd1apT91Z3p1LK3xFxjOf4LSK2kXLOq4j4jEucjXGOT5hhjowdPBrXH6eU0r94VWs9QHL3tRyxszVwMYLbI/gMuwn1O8gPXflnYlAyjvAUB7g3Pqi4xpXW48WY53cg09F4jPIP/tAOYH8E6xgz7Y2sg5e4wbCG7eN3rd30lvoS3Vpl63lzbjmC/TieYgsrvEPZWqvoLmAzT/BgY83X+0BZh9Iu6ZdWbl0wzFu+C94EQyu1dEdsv2B6RN5rYOqIBXFLXDFcMlzQXzFcEzNqbVCa0O2z9Zj725Td1+z9xfQJqfx/61gQnxo4XH4DRUPThLKPR+yeUqZ/MnnaqvpPL2PkCflXyuF3+hznlivqR0odB/MFfU8M1Gj/YE50uX2sXyNtHmIlgtt5W1dqcHruuu996AenQ++mMqimXbKTk73MYc4OuuReyW2jrLU8DCyX6uzW+RBOSq3enF04ifB+lZx2K7NIovamkezUsJc4iMEhDifFw8z9LplEqKvezWLp49B7X6uTL16L4e3tydldAAAAAElFTkSuQmCC'
    'en'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAMAAADTRh9nAAABgFBMVEUBG1SYl6efDSfKfooAG1cCLGy+n6UCIF0AE0YAGlSpL0LAlZynUmifDSeSkqLTiJjZ2dm7u7u2b3i/VG6vt8NGZJGUPUjFpq1AS2xRT3VeWoAAEkV5cImapbm6FDSjCh+mCSCflKR9hJa3EzMCKWmGeZLHDizJEjLCDCfkj5/afo7OIUEAF1TgeotTZpWpscjFvMwCK3DGGzXLFjjRPlbll6PliJrqp7Lonq2tuM9dcJkBIWTPL0rVW3HduMOjVHNhVH8qT4mwW3EWKmK7CyNGWYMxRXbLJkDt1Nvcc4P39/q8hZ/ck6DsydHIbYSGU3mWFSerqr6YhKWifJZ8T3Q7ToWZXmxfe6jBa4rayNTptb3Iz98VOXemnK5NUHW4tsuPmbHbipe5sMStpLfhhJO9fZFpWIbpqrWsGDdTW3a1bH+Ch6uPepXda33NT2GEg6bByNqvKkT77/IAET+HXoj67O7rusUoNFhtNkqMocKgbIHNw9IpR4Pcz9rV2eSdlqS0UDBzAAAAJnRSTlM1jXrzwaw9ta1CdaTTZ1XsbGzsvI2jvf6jvr6cVNT55/nUjeecVBCdCscAAAEnSURBVBjTHdADlsMAFADAX7trW1HjpE3T1LZtrG3j6vte5wgD+tnFJZ1Od6hS2+wmADAdmS2wMOOXG12Om2wfbGkNBsO6VfMHhabL3foIcWMBZRSlGwyP2DiE6Oem61WmBygqc7VC3snSJHChRsntfuz8oOj9aYA4G9DxPnjKmQxO4ZKEokk/nkwzd5U6XPpEr8OR8IoIkqKOnSMidVuEc0EQHA4BmUJ5HsOiRXjwUQTudFE3CCJ6EzyfEH11+Cxfyekc7s9hWFvCqexJ5foCYt8eViICQRbDfjsv7myPjjyB4vnC89WaMsYwJsa0iXC1/wYxdhQIRkhyEo0yZHzYchLhd+hprGsGo1a7t7M7JMmIp7QyvwwW8+Y0x25T728YjcZV1Zz+HwHdQPKkb2RfAAAAAElFTkSuQmCC'
    'es'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAYAAADkmO9VAAACmUlEQVQ4y22Ty25jRRCGv2r3sR1fZpJ4MmRAkBkWLBAoiH0WiCUPxcMgJN6DBRuEyEgsAI2GEZpLHHzie+zTp+tnYTsJEd0qVau79P1VrSr77pNPzzz7qbs/C+6PDXXkWMZTFrMsn7j7SNjIlUtzjTMs3FkJBVDXPR9JeubSafQ6f7tn9kXD7JAQzAQKjivgiIyRQ6PK7qUsjLL5OEuLbL6WCCZ1DDvCeDpx70W5f91tRKIZutmG2Cwn4PJmNjvO0nHGcNvcSwKEGSDjChEpMvtPE52jiqJXY9HBRK4gr4y0CKS5sZ4GqpmRFlBfbwQFEKH50GkOalJvTXzyVcnJaaL3OFH0akIUo2GH+TDQfSC6rTVpHqimgWoWqJdGWhpyIUGIovkg0zzIFKkmvnc2ZnACMWwkyzIwfFXw+/MOH36W+PzLJf33EwDapaXbs219SvDqDwjYNsg3/u27J5QvjOnkI8r1Ga//eYT0X9gOtIPhoK0F+Z1Ah+XVCYtJg8niAPnHXPx5jHwreC92Z7t3zxDuK68iNA8S9fgN899+RGqh/wPpNqu7WcYb9bApa//4HU1bY3tj+ipJH5xg3Mvu7hfcgecEsZoGLp8XVJcNVtPAejHFVdDKNa0Clr+OeD1s0eo7Rdcp2qJo6waYK1hNjPJt4MUvhv3wTV+Pxi3WF4HVLOC1cLYt0XbinhO7ugHGthPbm7YXoq7gemJcXQR+/lvENz+18dDA3Uk4rlugLYUtDRsZgYCJ7SSBpE21ElniWiJ5JrrE2PM8u/+V4bJGS3dlzNpIXWAfMTB0aNC07UiyBWaJWtI1Kl2cR5e+n3k+z9hLzC9dWtQ1TqPRdvduNvYNDrMxcPdBhIeG9SUVQkrSMsEwu7800/m/wsXxL/gOJaYAAAAASUVORK5CYII='
    'fr'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAYAAADkmO9VAAAB2UlEQVQ4jY2SPW4TURRGz33z7DhxEJFHQZRkAShUdNCwAdgBPYugoqBlD6yCkhVkBaG0EmxMYsfxeO79KDz+i+yIJ13Nm+bofPd7ll9/fBPYOeFnwLMIHRFh79++nH/99OF2Pp//lTSQNACG7j66+vJtMv7x816tSFFH1xWnCjuT/Dw7fCalV7LUQxgmsKBz2KUsS9wdd6/cfShpUNf16MbSpErMwi0lFUeGnWK8GKk+zop4R+4AQAgkIDg4OKQsSyTh7m13f94M43YbIyEJQyQSIIaITMQCIiCC9b8AMDNyzuScWZ7j3KLGEICBJOaIkMiEN5AN4HL2HElIgdDijohm8trqAbAx3AlkCWQN1cowwBtAPPg+ZoiaTS1AIRGwabgERVPO/sg0kM3IQvh6h/6glP/Y4SZsZagdhku7RyIvI24bxj7g9rPZW8pmw1rP7shNf49GbiJuxV4Zet3AHOQgMZtOGI1GFEWxetg5Z8yssYot0L1i49nU0zEev8CvibhD+Gx62+n3+92U0klKqUwp9YqiaOec+T2944981WyFNMWHgS4y6Dvz2QXSJebXeEywFFFVncFg0JV0Yma9iCiBstVqPe2Pb5/cyFsBqhV3lXRVo0u5XfwD3Lj3FBPR3T4AAAAASUVORK5CYII='
    'it'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAYAAADkmO9VAAAB40lEQVQ4jZWTvXLTQBRGv7u7/kkcJhlrwjBU5AnCQE3FC9CFt6GmoaSh5k1oKP0EofQk2DGOFWJL9/soVjI2iSHszB1pC505R1pZ+vD2FeGnEE8APqZhH6K9efqyev/i7Lqqqh+SJpImAKbuPrv4+KlcfPl6KzLQMXD4sZwnAk+Tw9+hF58rhiEggwSI6B/uoSgKuDvcfeXuU0mTuq5nc4vlSrakhRCi9o3hGMmezVZ+kCS+xl4XiAAggARk6A36KIoCkuDuXXd/0gwWvR4sRkiESQhmgISphAQSCEJrloHNHoCZIaWElBLaddDpog4BYpaQiEoCM9AzBA1wDSV2LZGQOyQ1Q7CZbEgCpt8gEaB2A6UMRQNk3nOdLN9Ibk3/AdyYbKcN4J1k/TUZDWQbTHhO9vuT/8NQdw09A7eSdxtSBPmHIe/7KFvJDzVs7QiyPTZq3mEL10OAGdTeayu5qgEnUHu+UljObzCbzRBjXB/slBLMLD9MQSDEbHdL30ieLxdwfkPllxBv4PLl1aI/Ho8HIYSjEEIRQhjGGLspJXwvS1x5lX9LEitRP+VTAqME4TPK5Qi1ncN5CdUlFMhy2Z9MJgNJR2Y2JFkAKDqdzuH4ev5oXlUdmlRTNyv6RS2dixr9AmYGJOT/Z+8EAAAAAElFTkSuQmCC'
    'ja'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAMAAADTRh9nAAAA5FBMVEXX19fQ0NDMzMzc3NzMzMzFxcXe3t7Pz8/h4eHMzMzl5eXb29va2trX19fZ2dnKysrLy8vGxsbAwMDAwMDZ2dne3t7Z2dnj4+Pi4uLX19fb29vT09Pe3t65ubn////7+/v6+vq9AC7x8PG7ACv+/f65ACm/ATPV1dW2ACfR0dH89/jv7+/29vbo6Ojbe5O+vr7GxsbKM1jqsb7s7Oz08vO7DzbBCTjPSWrDw8PBFj7stsTsusbgjaHX19fz8/O2AyrKysrt7e326OvOV3DNQGL9+vvtxc3EOlfNzc3aaojUboTg4ODg1fN6AAAAHnRSTlO1d6bUVDyuQfMz/bu+japwrqCNaWzEnOfUxOfs/r1uIjwUAAAA10lEQVQY01XQ15KCQBAF0CYJomLWTaIM44gIKEFds2PYNfz//8gAVmk/nltdfatBbpS0XE5rlbiyKEv5vCxyPBSUkCJEQ1dtfzQLAEVBucDGwTob+xA5EaIUbZ09DLqM8NK7eoGdxL0nrnxCiL8+JzjJcE6IYRiL5SviGTPTnGc4TJFRvz9lR3u/Keonk5n1jsEiJss6JuvjDO21v7N2/39vqNuBd/NWOMOLsx3RkI4idEi7YxSjIhRBg0/hS7mrruuqP98d4Dkx/o0ki/VyrcpXKtVaXXoAVAEniyY09XwAAAAASUVORK5CYII='
    'ko'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAMAAADTRh9nAAABNVBMVEXe3t7MzMzMzMzc3Nzd3d3S0tLIyMjMzMzFxcXc3NzV1dXX19fU1NTh4eHf39/Z2dnLy8vX19fa2trGxsbAwMDLy8ve3t7m5ubX19fl5eXT09PCwsK5ubnT09O7u7v////6+vv+/v77+/u9vb3CwsL29vbOMT3X19cAQptlZWXv7u/Q0NDt7e11dXWxsbGrq6uioqLx8vTg4ODw8PDl5eXGxsabm5u5ubn5+fm0tLRtbW6NjY3T09NpOGmZNFPppKkyPoUaP4/geYH88PHyzdA8bLGzx+H44OJahL3z8/Pr6+vKysra4/D56eqmM03IQ1LxyMqINVvRQEs/PX5oaGi/M0UjVqK/yd/bZ3CEpdDd5/IVU6XXV2FZWVmSrtSBgYFfX1/uvMDHx8fNzc1WVlfB0OZWVlaQJKoeAAAAH3RSTlPUM1Su/XJBpju8s7dB8+e9Z42gn42uxPnE7Hp6vexsd+Kq8QAAARNJREFUGNNFztdygkAAheGlKNh7erK7wiIQAUHEFntfTe+9mOT9HyGQyUzuznw35wfxUCbLcVw2s5Nm4yLDhNmQAFLbVDclndLNJNiIcLEISKxBiWDaMmoQFxvEps1mdUGWoFSElc9vGc/GMwyl1iuFaAmOfHxro/fh3bA3kq60CkQfAeqmU35SFOWihzzNgqgWIMTw9lpRTrqnx3yd/OO00z2/7HfO/AmRHCBpOg83/YGqDsYBNmRwWISW1kb3j6qqTkYls/yHnvYs2y/zyXxKPP+9IQXoGoYM8eprBestw/pFstCtOtVtgvx4o23jqgTWCZCKRmNbIMk7rmuaLr+/B4QQG2YYMcymd/M5oVDI5Q/EH+/7MZmhhNx7AAAAAElFTkSuQmCC'
    'nl'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAYAAADkmO9VAAACnElEQVQ4jY2TzW5cRRSEv3OnZ+Z6xiNPbGIZJwHCA5CwQyDYsGCFxIo3Yc3DIMQjsEA8QdgYiQULEiuKosSOx2PPjz33dlexGNtJJCRypJbqqHWqVN114qdPvviSogdyuV/Zu2EGlqLIbcGzVjqTy0k4TrLKJPC0hBedNl827lS4DEW5bem+xYOkoh8HEQ87dLbBERhXgRDFRhUU0xR5ouicFGsqaaHorCpUVcHAqm5HxEdntJvJ0teD1KUbYBtLGOGqWvcGiV4J75VKe0WBCBQVBrCJMISZZJMkUeHXZNIVkbHXOGw6mAqTAAgcYGItiGltZJOMqff32Ly7T3e8RaSEbbBo5wva83Oa0ymrySnN6ZS8vHhLzDZR9xndu8Puzpi09+03fPz5Z4w+vEfv1pjodgHAIi+WtOczmtPp+kzPaKZn5MUSl4yBqt+n/942vTvvs5Fb0t3vv2P/04ektDbzf1VWDWW5xKUAUNU13c0hbdvy4tEjUrVRvxPRdXX6PTr93n/eSSLZXju0mS8bLleZXETdS2zUibrffSch28wWl6SnL854fPw3z48XTKZLLlYtbS7UvUTdT2xt9hmPara3NhiPNhgN+wwHPTpVAHCxanl1Mufx05f89vsfpF9+/ZOjWc2zlzNWTUZ6/YNY9FLFeFRza6tmvFkzGvQZDrpUEdjictVy/GrG4bMjDv/5i/jgqx/cHexiB3orh4KrWPBGRG6wrvF6Rmpp58/XwS65RSqoZFB5Y/jqfQiCuNmMG8FrNyo4N1hlTZhXs7mVD13KsVSWtgqmBg+Bsc1OOLaNeybAYNY7aRWsbJVmYusggX/Oq/kBUZ4gH0fWIldWVVyDh8ZjF23b3rG1A7GFGRHuIlvKSzkfUfQE6eBfZNwTNPI35mgAAAAASUVORK5CYII='
    'pt'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAYAAADkmO9VAAACEUlEQVQ4jY3UT2uUVxTH8c/zzDPJNGNMnDBptAq1GApBVEpXFXHRZd3YfXHnvq+gr8IX4ELcu3bRhUVcSRRaAsW/YG3ixNiZyfy993qYh1FrHDxwuJd7D9/zO+dwb1b8WlyIMZ4VnZSsRnFBlOUxHx3uxfad694kWokWdgN76Ob0AznqaGacjJwtQgy/KZxLUgOZhEhMUR5oIjAM7CZaY/YS3cAgkecsVGhW+HqHQ0WK6UdVZCYWS0/kc1PgXGCt9GkIE4mVcv0XhVDepHeRFawv0Vyks8HCc+bbZtqoRBQfpCv3l05x5lHNUkFnhc7RqsbDtvnt2dB4EHC1xi+n+ebGss7ln4WN/3RGd8Uv247eJE+fBibkU2Dp3y7R77OW6nbXr3j+01WhWNQ7Rn91tsIPgWW3U+TeM7pHojB/2KB5wlxrrGjTO/45JYcSVqbYesXpBltfdXz3x035F/OGq6/F76nenq3uwKHsdPm7xe31N4Z/XfPDq5q9i7saL6k/+xyFB0z59ye8aAw9WNtzfquwdmug/pTKaDYwTUv+H3Cc+PMl24Ng+X5Q7ZHNmO7HCsdMexneNWQ8YLA/OSpKzz4B6nm/h/s6gieCHdG+KKAWB+r/sJyzktOoMFeYPLO8BAQMSd3Jx7FZiG7o2xQ9Fu0IuoKoUMt66tssZzQiK1ipspSzWKEaSSP2B2wHHic23wLmpgDwKBqqjgAAAABJRU5ErkJggg=='
    'ru'      = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAYAAADkmO9VAAAClUlEQVQ4W22Sy24jVRCGv3PpTsd2MrGTGYHECOYB0LBGgg0vwdvwGKxZ8AhIbFgiFrPKCiIEGWYWkDh2nKTtuC9VPws3c2NK+lXn6Jz66qIKZ2dnXwBPgScxxkc555G7B0mdmd11XXcjaSFpASzNbAWsY4xbM4vAGHgYQnji7k8z8M1kMvmsLMtZSinEGAFwd9wdM8PMWjNbSlr0fb+StDazRlKMMY5SSg9TSp/M5/NJdvevptMpVVURQuBdk4SZlWb2waBXyQBijKSUiDFycXFBlkRRFO+FAYQQyDmTc37v+3/WdR2SyO7iz3/WvJg3zFcNTecgCMDBfuZoUjA7LDk5LJkdloyq/4PX9z2//3XNjz+/JH/30wv+3tacX2yZX29pWgOHiDioMg/GBccHBceHJbODgumkZFwlUgoguN92XC7u+ePlNc+e/Ub+9ofn1KxBEdxBAh8kgXjjLMocGO8lcgQkNtuWdd2CeljfketNB5VA/jboXT+obUS77Xb/pdfeHeRk3IdLZGIbKmtJMhrP3JNpVOyCeBv8qoA3wPt+T37cXfFpuOSj/pqp3bHnLdmNrRIbCm5DxQ37LBlxS0XNHrVKpF3LpRqmVvOhXTFqfyV/vf2FL/tbHtuSwns0zFESEjQkVmHEkhE37FNTUrM3TEKU3jL1mke25LK7In/envExiSQhH8QOiKBQxwk3nGg1dCu023h8aF+C1p0LF9nd6QwaF50L8wHGbhejRAQSEAfQa/CuABdsfZcgy51Fp7p3PTdpbmLjJiOqwhlHdBTgOKBZVCgjIgyL79oV0EnayJeGTrOk71e9n5rpXFHzYKx7w7vCq9AzJnAkNJNzLHSc4QEKB0EqPEi9a9O4Ll06dw+n/wKygA3Jb422RgAAAABJRU5ErkJggg=='
    'zh-Hans' = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAYAAADkmO9VAAACHklEQVQ4jWWTy24VRxRF16nu9jvCDxF5hIJQpoERM0ZMGPNDfA1/gJRR/oAMjBhFikBRIku+upaT4Fd3n70ZdN2H7Z5U1S7VOquOquP3X16+Inlu62lrfgxrJ6TgyfXQf938f1D+iz23maM8F+Uiw5cxjDfFKrJ2Ez0OxVMrn7eRvNuNeNFRDtvj2+C0gxKU46T/K0gaZPejdO4o89G+sHw5BrdBlIiy0ygeR+inOd5rJb3e32vYfnLLxpsZw2/75KdtynwTfu7xf8Fw2m5kxHGWcpwSwigCgDA0ARGF2WhaSXQb0L2Z0b09Y/y4A9pEf3aUZ1eQhcamAcBMX+AKxGDMYCOb1hK+LkSTDL8eoL9bSIGNe3APKMGeMhtksKZ5zWwjKpBrGD4coH86KIklyrMbkKY7iRWsAo0mu0VW91tbWEl+3gIJuwGJOOgZP24TKSzdNbQB18GYamiqoar+4qCE/uig912DqWEVtp5N46qHS4MV3LMyra3KWLPiHmwN3rpCFsAlXKsDD0HrBVbQpeEC6DXDh6DpeSyh6wWqzB3gIrR1r091/qB/3LnyytCiz0QWQ61iQ2AC09Q/oQWa+1awBF2vv8OZh29pfx3MLK0r7ARvRbBbxH6EjxrHYcEbDVAwUXs52gy2L5Xntk9aw/vzzBOFv8gxg7zUGBobtsLjrrPsR9GhiSOnj9rwo4AfCnQCj9LVDT6T/UWZJ98BB7RLvvMNl40AAAAASUVORK5CYII='
    'zh-Hant' = 'iVBORw0KGgoAAAANSUhEUgAAABQAAAAPCAYAAADkmO9VAAAB40lEQVQ4jaXTz2pTQRzF8c+de41tolhTKhUX/gFxqXQrQqHv4mvoukvfwYXP0E1duRCFoghuVFCIWhsq7U3T5M6MiySSCqWoB87mN8z3nIH5FVX15EFK6S7xJuWVdju067opQkjjGOMB459B2nvt8V6Q+pF91IFhJKCDlczNzN0qxvSI1r2cy2632yrW1rrG4+jly12DwQhJoRktyf2Cvch+pM4cZ0KgHVgpubHLhSrntEFbUVTW1695+PC2lLLNzTe2t3vIiK0VVjOrCXE6hQKlSdVvqEgIOp1gbe2yjY2rRqNka+uLFy++OT5uwPnpxZmykxpPZ1NgUhRZrzfw6tUPTZN8/nxodjbx6bCZ0jywrse2t3u+fq0dHTXevu1P22V5ijgNNB9UkZQa19K+i+8/OPo0MhxG14vgknP6Fv20eCbsd8N179x34JYfrjQHOs0xspHSofP62vYtKs5A5tkLnrmaa2XOnHD6Sw/Jz8nVHT2tP5L+VQlhHvA/sN/AGeh/YTNOlTHCEI3JFjDZgGDymas5F6c0O5oHfucw8qlht2FQEDMLgU5gKbBc0i1pVdOgMAXESaF8SD+xU+HpHjuRj4ndkjqRRiyUdDJLBd3EMpYrLpVcLDiXyA2DId8bPmLnFwByBs8DRkw6AAAAAElFTkSuQmCC'
}
$script:FlagBitmaps = @{}
function Get-FlagBitmap([string]$Code) {
    if ($script:FlagBitmaps.ContainsKey($Code)) { return $script:FlagBitmaps[$Code] }
    if ($script:FlagB64.ContainsKey($Code)) {
        try {
            $bytes = [System.Convert]::FromBase64String($script:FlagB64[$Code])
            $ms  = New-Object System.IO.MemoryStream($bytes, 0, $bytes.Length)
            $bmp = [System.Drawing.Bitmap]::FromStream($ms)
            $script:FlagBitmaps[$Code] = $bmp
            return $bmp
        } catch { }
    }
    return $null
}

# ---- theme (matches Legacy Downloader) ----
$FontBase  = New-Object System.Drawing.Font('Segoe UI', 9)
$FontBold  = New-Object System.Drawing.Font('Segoe UI', 9, [System.Drawing.FontStyle]::Bold)
$FontTitle = New-Object System.Drawing.Font('Segoe UI', 13, [System.Drawing.FontStyle]::Bold)
$FontMono  = New-Object System.Drawing.Font('Consolas', 9)
$ColBg     = [System.Drawing.Color]::FromArgb(246, 248, 250)
$ColCard   = [System.Drawing.Color]::White
$ColPrim   = [System.Drawing.Color]::FromArgb(18, 98, 200)
$ColText   = [System.Drawing.Color]::FromArgb(30, 41, 59)
$ColBorder = [System.Drawing.Color]::FromArgb(203, 213, 225)
$ColMuted  = [System.Drawing.Color]::FromArgb(100, 116, 139)
$ColWarn   = [System.Drawing.Color]::FromArgb(180, 83, 9)
$ColOk     = [System.Drawing.Color]::FromArgb(21, 128, 61)

$ShimVer = Get-DllVersion $ShimSrc

function New-Btn([string]$Text, [int]$X, [int]$Y, [int]$W, [int]$H = 30, [bool]$Primary = $false) {
    $b = New-Object System.Windows.Forms.Button
    $b.Text = $Text; $b.SetBounds($X, $Y, $W, $H)
    $b.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
    $b.Cursor = [System.Windows.Forms.Cursors]::Hand
    if ($Primary) {
        $b.Font = $FontBold; $b.BackColor = $ColPrim; $b.ForeColor = [System.Drawing.Color]::White
        $b.FlatAppearance.BorderSize = 0
    } else {
        $b.Font = $FontBase; $b.BackColor = $ColCard; $b.ForeColor = $ColText
        $b.FlatAppearance.BorderColor = $ColBorder; $b.FlatAppearance.BorderSize = 1
    }
    return $b
}
function Msg([string]$Text, $Icon) {
    [System.Windows.Forms.MessageBox]::Show($Form, $Text, (T 'app.msgbox_title'),
        [System.Windows.Forms.MessageBoxButtons]::OK, $Icon) | Out-Null
}

if (-not (Test-Path -LiteralPath $ShimSrc)) {
    [System.Windows.Forms.MessageBox]::Show((T 'dlg.no_shim'),
        (T 'app.msgbox_title'), 'OK', 'Error') | Out-Null
    exit 1
}

$script:RuntimeDetected = Test-KinectRuntime

# ---------------------------------------------------------------------------
# form
# ---------------------------------------------------------------------------
$Form = New-Object System.Windows.Forms.Form
$Form.Text = (T 'app.window_title')
$Form.ClientSize = New-Object System.Drawing.Size(600, 612)
$Form.StartPosition = 'CenterScreen'
$Form.FormBorderStyle = 'FixedSingle'
$Form.MaximizeBox = $false
$Form.BackColor = $ColBg
$Form.Font = $FontBase
$Form.ShowIcon = $false   # WinForms' default per-form icon, not a real KinectNavigator logo

$lblTitle = New-Object System.Windows.Forms.Label
$lblTitle.Text = (T 'app.title'); $lblTitle.Font = $FontTitle; $lblTitle.ForeColor = $ColText
$lblTitle.SetBounds(20, 16, 300, 30); $Form.Controls.Add($lblTitle)

$lblSub = New-Object System.Windows.Forms.Label
$lblSub.Text = (T 'app.subtitle')
$lblSub.ForeColor = $ColMuted; $lblSub.SetBounds(22, 74, 560, 18); $Form.Controls.Add($lblSub)

$lblVer = New-Object System.Windows.Forms.Label
$lblVer.Text = $(if ($ShimVer) { T 'app.installer_version' @{ v = (VerShort $ShimVer) } } else { '' })
$lblVer.ForeColor = $ColMuted; $lblVer.TextAlign = 'TopRight'
$lblVer.SetBounds(360, 52, 220, 18); $Form.Controls.Add($lblVer)

# ---- language picker (owner-draw: flag + native name), top-right ----
$lblLang = New-Object System.Windows.Forms.Label
$lblLang.Text = (T 'app.lang_label'); $lblLang.ForeColor = $ColMuted
$lblLang.TextAlign = [System.Drawing.ContentAlignment]::MiddleRight
$lblLang.SetBounds(300, 14, 74, 26); $Form.Controls.Add($lblLang)

$cmbLang = New-Object System.Windows.Forms.ComboBox
$cmbLang.DropDownStyle = [System.Windows.Forms.ComboBoxStyle]::DropDownList
$cmbLang.DrawMode = [System.Windows.Forms.DrawMode]::OwnerDrawFixed
$cmbLang.ItemHeight = 22
$cmbLang.Font = $FontBase
$cmbLang.SetBounds(380, 12, 200, 26)
$cmbLang.BackColor = $ColCard; $cmbLang.ForeColor = $ColText
$script:Langs = @(Get-AvailableLanguages)
if ($script:Langs.Count -gt 0) {
    $cmbLang.Add_DrawItem({
        param($s, $e)
        if ($e.Index -lt 0 -or $e.Index -ge $script:Langs.Count) { return }
        $e.DrawBackground()
        $lang = $script:Langs[$e.Index]
        $bmp  = Get-FlagBitmap $lang.Code
        $flagX = $e.Bounds.X + 6
        $flagY = $e.Bounds.Y + [Math]::Max(0, [int](($e.Bounds.Height - 15) / 2))
        if ($bmp) {
            $e.Graphics.DrawImage($bmp, $flagX, $flagY, 20, 15)
            $borderPen = New-Object System.Drawing.Pen($ColBorder)
            $e.Graphics.DrawRectangle($borderPen, $flagX - 1, $flagY - 1, 21, 16)
            $borderPen.Dispose()
        }
        $textX  = $flagX + 28
        $brush  = New-Object System.Drawing.SolidBrush($e.ForeColor)
        $sf     = New-Object System.Drawing.StringFormat
        $sf.LineAlignment = [System.Drawing.StringAlignment]::Center
        $tb = New-Object System.Drawing.RectangleF($textX, $e.Bounds.Y, ($e.Bounds.Width - $textX), $e.Bounds.Height)
        $e.Graphics.DrawString($lang.NativeName, $e.Font, $brush, $tb, $sf)
        $brush.Dispose()
        $e.DrawFocusRectangle()
    })
    foreach ($l in $script:Langs) { [void]$cmbLang.Items.Add($l.NativeName) }
    $curLang = Get-CurrentLanguage
    for ($i = 0; $i -lt $script:Langs.Count; $i++) {
        if ($script:Langs[$i].Code -eq $curLang) { $cmbLang.SelectedIndex = $i; break }
    }
    if ($cmbLang.SelectedIndex -lt 0) { $cmbLang.SelectedIndex = 0 }
    $Form.Controls.AddRange(@($lblLang, $cmbLang))
} else {
    $lblLang.Visible = $false
}

$lblGF = New-Object System.Windows.Forms.Label
$lblGF.Text = (T 'gf.label'); $lblGF.Font = $FontBold; $lblGF.ForeColor = $ColText
$lblGF.SetBounds(20, 100, 300, 20); $Form.Controls.Add($lblGF)

$txtGF = New-Object System.Windows.Forms.TextBox
$txtGF.SetBounds(20, 122, 455, 24); $txtGF.Font = $FontBase; $Form.Controls.Add($txtGF)

$btnBrowse = New-Btn (T 'btn.browse') 483 121 97 26
$Form.Controls.Add($btnBrowse)

$pnl = New-Object System.Windows.Forms.Panel
$pnl.SetBounds(20, 158, 560, 112); $pnl.BackColor = $ColCard; $pnl.BorderStyle = 'FixedSingle'
$Form.Controls.Add($pnl)
$lblStatus = New-Object System.Windows.Forms.Label
$lblStatus.SetBounds(14, 10, 532, 92); $lblStatus.Font = $FontBase; $lblStatus.ForeColor = $ColText
$pnl.Controls.Add($lblStatus)

$lnkRuntime = New-Object System.Windows.Forms.LinkLabel
$lnkRuntime.SetBounds(20, 278, 560, 20); $lnkRuntime.Font = $FontBase
$lnkRuntime.LinkBehavior = [System.Windows.Forms.LinkBehavior]::HoverUnderline
$Form.Controls.Add($lnkRuntime)
$lnkRuntime.Add_LinkClicked({ Start-Process $RuntimeUrl })

$lblOpt = New-Object System.Windows.Forms.Label
$lblOpt.Text = (T 'opt.header')
$lblOpt.Font = $FontBold; $lblOpt.ForeColor = $ColText
$lblOpt.SetBounds(20, 306, 560, 20); $Form.Controls.Add($lblOpt)

$pnlOpt = New-Object System.Windows.Forms.Panel
$pnlOpt.SetBounds(20, 328, 560, 112); $pnlOpt.BackColor = $ColCard; $pnlOpt.BorderStyle = 'FixedSingle'
$Form.Controls.Add($pnlOpt)

$lblHand = New-Object System.Windows.Forms.Label
$lblHand.Text = (T 'opt.hand_label'); $lblHand.ForeColor = $ColText
$lblHand.SetBounds(14, 15, 108, 20); $pnlOpt.Controls.Add($lblHand)

$cmbHand = New-Object System.Windows.Forms.ComboBox
$cmbHand.DropDownStyle = 'DropDownList'
$cmbHand.SetBounds(124, 12, 170, 24)
[void]$cmbHand.Items.Add((T 'opt.hand_right'))
[void]$cmbHand.Items.Add((T 'opt.hand_left'))
$pnlOpt.Controls.Add($cmbHand)

$chkMirror = New-Object System.Windows.Forms.CheckBox
$chkMirror.Text = (T 'opt.mirror'); $chkMirror.ForeColor = $ColText
$chkMirror.SetBounds(320, 13, 230, 22); $pnlOpt.Controls.Add($chkMirror)

$chkBack = New-Object System.Windows.Forms.CheckBox
$chkBack.Text = (T 'opt.back'); $chkBack.ForeColor = $ColText
$chkBack.SetBounds(14, 44, 290, 22); $pnlOpt.Controls.Add($chkBack)

$chkHud = New-Object System.Windows.Forms.CheckBox
$chkHud.AutoSize = $false
$chkHud.Text = (T 'opt.hud'); $chkHud.ForeColor = $ColText
$chkHud.SetBounds(320, 44, 230, 22); $pnlOpt.Controls.Add($chkHud)

$btnMore     = New-Btn (T 'btn.more')       14 74 130 27
$btnOpenIni  = New-Btn (T 'btn.open_ini')  152 74 160 27
$btnResetCfg = New-Btn (T 'btn.reset_cfg') 320 74 150 27
$pnlOpt.Controls.AddRange(@($btnMore, $btnOpenIni, $btnResetCfg))

$btnInstall   = New-Btn (T 'btn.install')    20 454 130 34 $true
$btnUninstall = New-Btn (T 'btn.uninstall') 160 454 130 34
$btnHelp      = New-Btn (T 'btn.gestures')  350 454 100 34
$btnClose     = New-Btn (T 'btn.close')     460 454 100 34
$Form.Controls.AddRange(@($btnInstall, $btnUninstall, $btnHelp, $btnClose))

$txtLog = New-Object System.Windows.Forms.TextBox
$txtLog.SetBounds(20, 498, 560, 96); $txtLog.Multiline = $true; $txtLog.ReadOnly = $true
$txtLog.ScrollBars = 'Vertical'; $txtLog.BackColor = [System.Drawing.Color]::White; $txtLog.Font = $FontMono
$Form.Controls.Add($txtLog)

function Write-Log([string]$s) {
    $txtLog.AppendText($s + "`r`n")
    $txtLog.SelectionStart = $txtLog.TextLength; $txtLog.ScrollToCaret()
}
function Log-IniSet([string]$k, [string]$v) { Write-Log (T 'log.ini_set' @{ k = $k; v = $v }) }
function Write-ActionResult($r) {
    if ($r.Vars) { Write-Log (T $r.Key $r.Vars) } else { Write-Log (T $r.Key) }
}

function Get-IniPath {
    $st = $script:CurState
    if ($null -eq $st) { return $null }
    if ($st.state -ne 'genuine' -and $st.state -ne 'installed') { return $null }
    return (Join-Path $st.folder 'kinectnav.ini')
}

function Populate-Config([string]$gf) {
    $script:Loading = $true
    try {
        $ini = Read-IniMap (Join-Path $gf 'kinectnav.ini')
        $h = ([string](Get-IniVal $ini 'handedness' 'right')).Trim().ToLower()
        $cmbHand.SelectedIndex = $(if ($h -eq 'left' -or $h -eq '1') { 1 } else { 0 })
        $chkMirror.Checked = (IniBool (Get-IniVal $ini 'mirror' '0'))
        $chkBack.Checked   = (IniBool (Get-IniVal $ini 'enable_back' '1'))
        $chkHud.Checked    = (IniBool (Get-IniVal $ini 'overlay' '0'))
    } finally {
        $script:Loading = $false
    }
}

function Refresh-Status {
    $st = Get-State ($txtGF.Text.Trim())
    $script:CurState = $st
    $detail = $(if ($st.detailVars) { T $st.detailKey $st.detailVars } else { T $st.detailKey })
    $lines = @()
    switch ($st.state) {
        'genuine'   { $lines += '[ OK ]  ' + $detail }
        'installed' { $lines += '[ OK ]  ' + $detail }
        'foreign'   { $lines += '[ !! ]  ' + $detail }
        'nogame'    { $lines += '[ X  ]  ' + $detail }
        'nodll'     { $lines += '[ X  ]  ' + $detail }
        default     { $lines += '        ' + $detail }
    }

    $canInstall = ($st.state -eq 'genuine' -or $st.state -eq 'installed')
    $btnUninstall.Enabled = ($st.state -eq 'installed')

    if ($st.state -eq 'installed') {
        $iv = $st.instVer
        if ($null -ne $ShimVer -and $null -ne $iv) {
            if ($ShimVer -gt $iv) {
                $btnInstall.Text = (T 'btn.update'); $btnInstall.Enabled = $true
                $lines += '[ i  ]  ' + (T 'ver.update_avail' @{ inst = (VerShort $iv); shim = (VerShort $ShimVer) })
            } elseif ($ShimVer -eq $iv) {
                $btnInstall.Text = (T 'btn.uptodate'); $btnInstall.Enabled = $false
                $lines += '[ OK ]  ' + (T 'ver.uptodate' @{ inst = (VerShort $iv) })
            } else {
                $btnInstall.Text = (T 'btn.reinstall'); $btnInstall.Enabled = $true
                $lines += '[ !! ]  ' + (T 'ver.older' @{ inst = (VerShort $iv); shim = (VerShort $ShimVer) })
            }
        } else {
            $btnInstall.Text = (T 'btn.update'); $btnInstall.Enabled = $true
            $lines += '[ i  ]  ' + (T 'ver.unreadable')
        }
    } else {
        $btnInstall.Text = (T 'btn.install'); $btnInstall.Enabled = $canInstall
    }

    if ($st.fs -eq 1)     { $lines += '[ i  ]  ' + (T 'fs.fullscreen') }
    elseif ($st.fs -eq 0) { $lines += '[ i  ]  ' + (T 'fs.windowed') }
    $lblStatus.Text = ($lines -join "`r`n")

    foreach ($c in @($cmbHand, $chkMirror, $chkBack, $chkHud, $btnMore, $btnOpenIni, $btnResetCfg)) {
        $c.Enabled = $canInstall
    }
    if ($canInstall) {
        Save-Config $Paths.ConfigPath $st.folder (Get-CurrentLanguage)
        Populate-Config $st.folder
    }
}

# re-apply every static string after a language change
function Apply-Language {
    $Form.Text     = (T 'app.window_title')
    $lblTitle.Text = (T 'app.title')
    $lblSub.Text   = (T 'app.subtitle')
    if ($ShimVer) { $lblVer.Text = (T 'app.installer_version' @{ v = (VerShort $ShimVer) }) }
    $lblLang.Text  = (T 'app.lang_label')
    $lblGF.Text    = (T 'gf.label')
    $btnBrowse.Text = (T 'btn.browse')
    if ($script:RuntimeDetected) {
        $lnkRuntime.Text = (T 'runtime.detected')
        $lnkRuntime.LinkColor = $ColMuted; $lnkRuntime.ActiveLinkColor = $ColMuted
        $lnkRuntime.DisabledLinkColor = $ColMuted; $lnkRuntime.Enabled = $false
    } else {
        $lnkRuntime.Text = (T 'runtime.missing'); $lnkRuntime.LinkColor = $ColWarn
    }
    $lblOpt.Text  = (T 'opt.header')
    $lblHand.Text = (T 'opt.hand_label')
    $hi = $cmbHand.SelectedIndex
    $script:Loading = $true
    $cmbHand.Items.Clear()
    [void]$cmbHand.Items.Add((T 'opt.hand_right'))
    [void]$cmbHand.Items.Add((T 'opt.hand_left'))
    $cmbHand.SelectedIndex = $(if ($hi -ge 0) { $hi } else { 0 })
    $script:Loading = $false
    $chkMirror.Text   = (T 'opt.mirror')
    $chkBack.Text     = (T 'opt.back')
    $chkHud.Text      = (T 'opt.hud')
    $btnMore.Text     = (T 'btn.more')
    $btnOpenIni.Text  = (T 'btn.open_ini')
    $btnResetCfg.Text = (T 'btn.reset_cfg')
    $btnUninstall.Text = (T 'btn.uninstall')
    $btnHelp.Text     = (T 'btn.gestures')
    $btnClose.Text    = (T 'btn.close')
    Refresh-Status
}

if ($script:Langs.Count -gt 0) {
    $cmbLang.Add_SelectedIndexChanged({
        $i = $cmbLang.SelectedIndex
        if ($i -lt 0 -or $i -ge $script:Langs.Count) { return }
        $code = $script:Langs[$i].Code
        if ($code -eq (Get-CurrentLanguage)) { return }
        $null = Initialize-Language -Code $code
        Save-Config $Paths.ConfigPath ($txtGF.Text.Trim()) (Get-CurrentLanguage)
        Apply-Language
    })
}

$btnBrowse.Add_Click({
    $d = New-Object System.Windows.Forms.FolderBrowserDialog
    $d.Description = (T 'gf.browse_desc')
    $d.ShowNewFolderButton = $false
    $cur = $txtGF.Text.Trim()
    if ($cur -and (Test-Path -LiteralPath $cur)) { $d.SelectedPath = $cur }
    if ($d.ShowDialog($Form) -eq [System.Windows.Forms.DialogResult]::OK) {
        $txtGF.Text = $d.SelectedPath.TrimEnd('\')
    }
})
$txtGF.Add_TextChanged({ Refresh-Status })

# ---- live config handlers (main window) ----
$cmbHand.Add_SelectedIndexChanged({
    if ($script:Loading) { return }
    $ini = Get-IniPath; if ([string]::IsNullOrWhiteSpace($ini)) { return }
    $val = if ($cmbHand.SelectedIndex -eq 1) { 'left' } else { 'right' }
    Set-IniKey $ini 'handedness' $val
    Log-IniSet 'handedness' $val
})
$chkMirror.Add_CheckedChanged({
    if ($script:Loading) { return }
    $ini = Get-IniPath; if ([string]::IsNullOrWhiteSpace($ini)) { return }
    $v = if ($chkMirror.Checked) { '1' } else { '0' }
    Set-IniKey $ini 'mirror' $v
    Log-IniSet 'mirror' $v
})
$chkBack.Add_CheckedChanged({
    if ($script:Loading) { return }
    $ini = Get-IniPath; if ([string]::IsNullOrWhiteSpace($ini)) { return }
    $v = if ($chkBack.Checked) { '1' } else { '0' }
    Set-IniKey $ini 'enable_back' $v
    Log-IniSet 'enable_back' $v
})
$chkHud.Add_CheckedChanged({
    if ($script:Loading) { return }
    $st = $script:CurState
    $ini = Get-IniPath; if ([string]::IsNullOrWhiteSpace($ini)) { return }
    if ($chkHud.Checked) {
        Set-IniKey $ini 'overlay' '1'
        Write-Log (T 'log.hud_on')
        if ($st.fs -eq 1) {
            $r = [System.Windows.Forms.MessageBox]::Show($Form, (T 'dlg.hud_fs_body'),
                (T 'app.msgbox_title'), 'YesNo', 'Question')
            if ($r -eq [System.Windows.Forms.DialogResult]::Yes) {
                if (Set-ConfigWindowed $st.folder) { Write-Log (T 'log.fs0_ok') }
                else { Write-Log (T 'log.fs0_fail') }
                Refresh-Status
            }
        }
    } else {
        Set-IniKey $ini 'overlay' '0'
        Write-Log (T 'log.hud_off')
    }
})

$btnOpenIni.Add_Click({
    $ini = Get-IniPath; if ([string]::IsNullOrWhiteSpace($ini)) { return }
    if (-not (Test-Path -LiteralPath $ini)) {
        if (Test-Path -LiteralPath $ExampleIni) {
            Copy-Item -LiteralPath $ExampleIni -Destination $ini -Force
            Write-Log (T 'log.ini_created')
        } else {
            Set-IniKey $ini 'enable_back' '1'
        }
    }
    Start-Process notepad.exe -ArgumentList $ini
    Write-Log (T 'log.ini_opened')
})

$btnResetCfg.Add_Click({
    $ini = Get-IniPath; if ([string]::IsNullOrWhiteSpace($ini)) { return }
    if (-not (Test-Path -LiteralPath $ini)) { Write-Log (T 'log.ini_none'); return }
    $r = [System.Windows.Forms.MessageBox]::Show($Form, (T 'dlg.reset_cfg_body'),
        (T 'app.msgbox_title'), 'YesNo', 'Warning')
    if ($r -eq [System.Windows.Forms.DialogResult]::Yes) {
        Remove-Item -LiteralPath $ini -Force
        Write-Log (T 'log.ini_deleted')
        Refresh-Status
    }
})

function Update-KeyBoxes($iniPath, $boxes) {
    $m = Read-IniMap $iniPath
    foreach ($nm in $KEY_ORDER) {
        $def = $KEY_DEFS[$nm]
        $cur = Get-IniVal $m $def[0] $def[1]
        $vk  = ConvertTo-Vk ([string]$cur)
        $boxes[$nm].Text = $(if ($null -ne $vk) { VkName $vk } else { [string]$cur })
    }
}

# ---------------------------------------------------------------------------
# "press a key" capture dialog
# ---------------------------------------------------------------------------
function Capture-Key {
    $d = New-Object System.Windows.Forms.Form
    $d.Text = (T 'capture.title')
    $d.ClientSize = New-Object System.Drawing.Size(340, 128)
    $d.FormBorderStyle = 'FixedDialog'; $d.StartPosition = 'CenterParent'
    $d.MaximizeBox = $false; $d.MinimizeBox = $false; $d.KeyPreview = $true
    $d.BackColor = $ColBg; $d.Font = $FontBase; $d.ShowIcon = $false
    $l = New-Object System.Windows.Forms.Label
    $l.Text = (T 'capture.body')
    $l.SetBounds(16, 16, 308, 44); $d.Controls.Add($l)
    $c = New-Btn (T 'btn.cancel') 232 78 92 28
    $c.Add_Click({ $script:capVk = $null; $d.Close() })
    $d.Controls.Add($c)
    $script:capVk = $null
    $ignore = @(16, 17, 18, 91, 92, 20, 144, 145)
    $d.Add_KeyDown({
        $vk = [int]$_.KeyCode
        if ($ignore -contains $vk) { return }
        $script:capVk = $vk
        $_.SuppressKeyPress = $true
        $d.Close()
    })
    $d.ShowDialog($Form) | Out-Null
    return $script:capVk
}

# ---------------------------------------------------------------------------
# "more settings" dialog (presets + clutch + key bindings)
# ---------------------------------------------------------------------------
function Show-MoreSettings {
    $ini = Get-IniPath
    if ([string]::IsNullOrWhiteSpace($ini)) { return }
    $iniMap = Read-IniMap $ini

    $d = New-Object System.Windows.Forms.Form
    $d.Text = (T 'more.title')
    $d.ClientSize = New-Object System.Drawing.Size(474, 520)
    $d.FormBorderStyle = 'FixedDialog'; $d.StartPosition = 'CenterParent'
    $d.MaximizeBox = $false; $d.MinimizeBox = $false
    $d.BackColor = $ColBg; $d.Font = $FontBase; $d.ShowIcon = $false

    $script:dlgLoading = $true

    $lf = New-Object System.Windows.Forms.Label
    $lf.Text = (T 'more.feel_header')
    $lf.Font = $FontBold; $lf.SetBounds(16, 14, 444, 20); $d.Controls.Add($lf)

    function New-PresetRow([string]$text, [int]$y, [string]$prefix, [string[]]$ids) {
        $lbl = New-Object System.Windows.Forms.Label
        $lbl.Text = $text; $lbl.SetBounds(16, ($y + 3), 210, 20); $d.Controls.Add($lbl)
        $cb = New-Object System.Windows.Forms.ComboBox
        $cb.DropDownStyle = 'DropDownList'; $cb.SetBounds(232, $y, 226, 24)
        $cb.Tag = @{ prefix = $prefix; ids = $ids }
        foreach ($id in $ids) { [void]$cb.Items.Add((T "$prefix.$id")) }
        $d.Controls.Add($cb)
        return $cb
    }

    $cbReach  = New-PresetRow (T 'more.reach')    40  'preset.reach'  $REACH_IDS
    $cbScroll = New-PresetRow (T 'more.scroll')   72  'preset.scroll' $SCROLL_IDS
    $cbCmd    = New-PresetRow (T 'more.cmdreach') 104 'preset.cmd'    $CMD_IDS
    $cbHold   = New-PresetRow (T 'more.hold')     136 'preset.hold'   $HOLD_IDS

    $chkClutch = New-Object System.Windows.Forms.CheckBox
    $chkClutch.Text = (T 'more.clutch')
    $chkClutch.SetBounds(16, 174, 444, 22); $d.Controls.Add($chkClutch)

    $lk = New-Object System.Windows.Forms.Label
    $lk.Text = (T 'more.keys_header'); $lk.Font = $FontBold; $lk.SetBounds(16, 210, 444, 20); $d.Controls.Add($lk)

    $keyBoxes = @{}
    $ri = 0
    foreach ($nm in $KEY_ORDER) {
        $ky = 236 + $ri * 30
        $lb = New-Object System.Windows.Forms.Label
        $lb.Text = (T ('key.' + $nm.ToLower())); $lb.SetBounds(16, ($ky + 4), 84, 20); $d.Controls.Add($lb)
        $tb = New-Object System.Windows.Forms.TextBox
        $tb.ReadOnly = $true; $tb.SetBounds(104, $ky, 226, 24); $tb.Font = $FontMono
        $d.Controls.Add($tb); $keyBoxes[$nm] = $tb
        $bc = New-Btn (T 'btn.change') 338 ($ky - 1) 120 26
        $bc.Tag = $nm
        $bc.Add_Click({
            $name = $this.Tag
            $vk = Capture-Key
            if ($null -ne $vk) {
                Set-IniKey $ini $KEY_DEFS[$name][0] ('0x{0:X2}' -f $vk)
                $keyBoxes[$name].Text = (VkName $vk)
                Log-IniSet $KEY_DEFS[$name][0] ('0x{0:X2}' -f $vk)
            }
        }.GetNewClosure())
        $d.Controls.Add($bc)
        $ri++
    }

    $bResetKeys = New-Btn (T 'btn.reset_keys') 16 430 110 28
    $bResetKeys.Add_Click({
        Remove-IniKeys $ini @('key_left', 'key_right', 'key_up', 'key_down', 'key_confirm', 'key_back')
        Update-KeyBoxes $ini $keyBoxes
        Write-Log (T 'log.keys_reset')
    }.GetNewClosure())

    $bOk = New-Btn (T 'btn.close') 384 472 74 28 $true
    $bOk.Add_Click({ $d.Close() })
    $d.Controls.AddRange(@($bResetKeys, $bOk))

    function Fill-Preset($cb, $table) {
        $meta = $cb.Tag
        $ids  = $meta.ids
        $cb.Items.Clear()
        foreach ($id in $ids) { [void]$cb.Items.Add((T "$($meta.prefix).$id")) }
        $m = Test-PresetMatch $iniMap $ids $table $CFG_DEFAULTS
        if ($m) { $cb.SelectedIndex = [array]::IndexOf($ids, $m) }
        else { [void]$cb.Items.Add((T 'preset.custom')); $cb.SelectedIndex = $cb.Items.Count - 1 }
    }
    Fill-Preset $cbReach  $PRESET_REACH
    Fill-Preset $cbScroll $PRESET_SCROLL
    Fill-Preset $cbCmd    $PRESET_CMD
    Fill-Preset $cbHold   $PRESET_HOLD
    $chkClutch.Checked = (IniBool (Get-IniVal $iniMap 'dpad_arm' '1'))
    Update-KeyBoxes $ini $keyBoxes

    # $ini is passed explicitly rather than relied on to leak in from Show-MoreSettings'
    # scope: Make-PresetHandler is itself a nested function, and .GetNewClosure() only
    # snapshots the DEFINING function's own local scope (its parameters included) -- it
    # does not reach through an extra function boundary to a grandparent scope. Passing
    # $ini as a parameter here puts it in that captured scope, same as $cb/$table/$labelKey
    # already are (which is why those always worked while $ini silently came through empty).
    function Make-PresetHandler($cb, $table, $labelKey, $iniPath) {
        return {
            if ($script:dlgLoading) { return }
            $meta = $cb.Tag
            $ids  = $meta.ids
            $i = $cb.SelectedIndex
            if ($i -lt 0 -or $i -ge $ids.Count) { return }   # "(custom)" row -> ignore
            $id = $ids[$i]
            foreach ($kv in $table[$id].GetEnumerator()) { Set-IniKey $iniPath $kv.Key ([string]$kv.Value) }
            while ($cb.Items.Count -gt $ids.Count) { $cb.Items.RemoveAt($cb.Items.Count - 1) }
            Write-Log (T 'log.preset_set' @{ label = (T $labelKey); name = (T "$($meta.prefix).$id") })
        }.GetNewClosure()
    }
    $cbReach.Add_SelectedIndexChanged( (Make-PresetHandler $cbReach  $PRESET_REACH  'more.reach'    $ini) )
    $cbScroll.Add_SelectedIndexChanged((Make-PresetHandler $cbScroll $PRESET_SCROLL 'more.scroll'   $ini) )
    $cbCmd.Add_SelectedIndexChanged(   (Make-PresetHandler $cbCmd    $PRESET_CMD    'more.cmdreach' $ini) )
    $cbHold.Add_SelectedIndexChanged(  (Make-PresetHandler $cbHold   $PRESET_HOLD   'more.hold'     $ini) )
    $chkClutch.Add_CheckedChanged({
        if ($script:dlgLoading) { return }
        $v = if ($chkClutch.Checked) { '1' } else { '0' }
        Set-IniKey $ini 'dpad_arm' $v
        Log-IniSet 'dpad_arm' $v
    }.GetNewClosure())

    $script:dlgLoading = $false
    $d.ShowDialog($Form) | Out-Null
}
$btnMore.Add_Click({ Show-MoreSettings })

# ---------------------------------------------------------------------------
# install / uninstall  -- both call the shared Core implementation
# ---------------------------------------------------------------------------
$btnInstall.Add_Click({
    $st = $script:CurState
    if ($null -eq $st -or ($st.state -ne 'genuine' -and $st.state -ne 'installed')) { return }
    if (Test-GameRunning) { Msg (T 'dlg.game_running') 'Warning'; return }
    $isUpdate = ($st.state -eq 'installed')
    try {
        $r = Install-KinectNavigator -GameFolder $st.folder -ShimSrc $ShimSrc -IsUpdate $isUpdate
        if (-not $r.Ok) { throw (T $r.Key $r.Vars) }
        if ($r.RenamedBackend) { Write-Log (T 'log.renamed_backend') }
        Write-ActionResult $r
        Refresh-Status

        $extra = ''
        if ($script:CurState.fs -eq 1 -and $chkHud.Checked) { $extra = (T 'dlg.install_ok_hud_note') }
        $head = if ($isUpdate) { T 'dlg.install_ok_upd' } else { T 'dlg.install_ok_new' }
        Msg ($head + [Environment]::NewLine + [Environment]::NewLine + (T 'dlg.install_ok_body') + $extra) 'Information'
    } catch {
        Write-Log (T 'log.error' @{ err = $_.Exception.Message })
        Msg (T 'dlg.install_fail' @{ err = $_.Exception.Message }) 'Error'
        Refresh-Status
    }
})

$btnUninstall.Add_Click({
    $st = $script:CurState
    if ($null -eq $st -or $st.state -ne 'installed') { return }
    if (Test-GameRunning) { Msg (T 'dlg.game_running') 'Warning'; return }
    try {
        $r = Uninstall-KinectNavigator -GameFolder $st.folder
        if (-not $r.Ok) { throw (T $r.Key $r.Vars) }
        Write-ActionResult $r
        Write-Log (T 'log.uninstalled_kept')
        Refresh-Status
        Msg (T 'dlg.uninstall_ok') 'Information'
    } catch {
        Write-Log (T 'log.error' @{ err = $_.Exception.Message })
        Msg (T 'dlg.uninstall_fail' @{ err = $_.Exception.Message }) 'Error'
        Refresh-Status
    }
})

# ---------------------------------------------------------------------------
# gestures help
# ---------------------------------------------------------------------------
$GesturesImg = $Paths.GesturesImg

function Show-GesturesText { Msg (T 'gestures.body') 'Information' }

$btnHelp.Add_Click({
    if (-not (Test-Path -LiteralPath $GesturesImg)) { Show-GesturesText; return }
    try {
        $img = [System.Drawing.Image]::FromFile($GesturesImg)
    } catch { Show-GesturesText; return }
    $gfm = New-Object System.Windows.Forms.Form
    $gfm.Text = (T 'gestures.title')
    $gfm.StartPosition = 'CenterParent'
    $gfm.FormBorderStyle = 'FixedSingle'
    $gfm.MaximizeBox = $false
    $gfm.BackColor = $ColBg
    $gfm.ShowIcon = $false
    $maxW = 900
    $sc = [Math]::Min(1.0, $maxW / $img.Width)
    $iw = [int]($img.Width * $sc); $ih = [int]($img.Height * $sc)
    $gfm.ClientSize = New-Object System.Drawing.Size ($iw + 24), ($ih + 60)
    $pb2 = New-Object System.Windows.Forms.PictureBox
    $pb2.SetBounds(12, 12, $iw, $ih)
    $pb2.SizeMode = 'Zoom'
    $pb2.Image = $img
    $gfm.Controls.Add($pb2)
    $ok = New-Btn (T 'btn.close') ($iw + 24 - 112) ($ih + 20) 100 30
    $ok.Add_Click({ $gfm.Close() })
    $gfm.Controls.Add($ok)
    $gfm.Add_FormClosed({ $img.Dispose() })
    $gfm.ShowDialog($Form) | Out-Null
})

$btnClose.Add_Click({ $Form.Close() })
$Form.Add_Shown({ $Form.Activate(); $Form.TopMost = $true; $Form.TopMost = $false })

# runtime link text (set here so Apply-Language can re-do it)
if ($script:RuntimeDetected) {
    $lnkRuntime.Text = (T 'runtime.detected')
    $lnkRuntime.LinkColor = $ColMuted; $lnkRuntime.ActiveLinkColor = $ColMuted
    $lnkRuntime.DisabledLinkColor = $ColMuted; $lnkRuntime.Enabled = $false
} else {
    $lnkRuntime.Text = (T 'runtime.missing'); $lnkRuntime.LinkColor = $ColWarn
}

$seed = $BootConfig.GamePath
if ([string]::IsNullOrWhiteSpace($seed)) {
    foreach ($g in @($ScriptDir, (Split-Path -Parent $ScriptDir))) {
        if ($g -and (Test-Path -LiteralPath (Join-Path $g 'legacy.exe'))) { $seed = $g; break }
    }
}
$txtGF.Text = $seed
Refresh-Status

[void][System.Windows.Forms.Application]::Run($Form)
