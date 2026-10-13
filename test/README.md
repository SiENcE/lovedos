# Tests
Each directory is a small LoveDOS game which checks part of the API and writes
the results to `log.txt`. `run.sh` runs them headless in DOSBox:

```
DPMI=/path/to/CWSDPMI.EXE test/run.sh            # all tests
DPMI=/path/to/HDPMI32.EXE test/run.sh conf11     # a single test
```

`EXE` selects the LoveDOS executable (default `bin/love.exe`), `DOSBOX` the
DOSBox command and `TIMEOUT` the seconds after which a test is aborted; set
`KEEP=1` to keep the temporary DOS directory for inspection. Tests use the
helpers in `testlib.lua`.
