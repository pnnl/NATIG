## How to know if there is an issue with the run

1. the command ` ps aux ` can give you details on how the run is doing when running an example of NATIG
2. the command ` tail output/ns3-helics-grid-dnp3<Example tag>.log ` allows you to see if the run is progressing or not. If after 10 minutes there are no changes to the output then there is an issue with the run. 
  1. Example tag options: ` -4G ` when running the 4G LTE example
  2. Example tag options: ` -5G ` when running the 5G example
  3. Example tag options: empty when running the 3G examples
3. the command ` tail output/gridlabd.log ` allows to see if there is any issues on the gridlabd side of the system
4. the command ` tail output/helics_broker.log ` allows you to see if there is any errors on the helics side. This file should remain empty unless there is an issue with the run

## Output examples when running previous commands

` ps aux `, when the run is working properly:

```
root@97323560c94f:/rd2c/integration/control# ps aux
USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
root         1  0.0  0.0   6052  3568 pts/0    Ss+  Aug26   0:00 bash
root         9  0.0  0.0   6676  4364 pts/1    Ss   Aug26   0:01 bash
root     86293  0.0  0.0   5788  3156 pts/1    S    01:46   0:00 bash run_experiment.sh
root     86295  0.0  0.0   9216  4588 pts/1    S    01:46   0:00 sudo bash run.sh /rd2c/ 5G RC 9500 conf
root     86296  0.0  0.0   5824  3076 pts/1    S    01:46   0:00 bash run.sh /rd2c/ 5G RC 9500 conf
root     86306  0.0  0.0  12996  9384 pts/1    S    01:46   0:00 /usr/local/bin/python /usr/local/bin/helics_broker --slowresponding --fed
root     86310  0.0  0.0   5824  1592 pts/1    S    01:46   0:00 bash run.sh /rd2c/ 5G RC 9500 conf
root     86316 15.2  3.2 1007564 469684 pts/1  Rl   01:46  19:46 gridlabd -D OUT_FOLDER=/rd2c/integration/control/output /rd2c/integration
root     86317 33.2  0.0 390188 10400 pts/1    Sl   01:46  43:14 /usr/local/lib/python3.6/site-packages/helics_apps/data/bin/helics_broker
root     86324  0.0  0.0   5824  1884 pts/1    S    01:46   0:00 bash run.sh /rd2c/ 5G RC 9500 conf
root     88258  0.0  0.3 133352 55640 pts/1    S    01:47   0:01 python3 ./waf --run scratch/ns3-helics-grid-dnp3-5G --helicsConfig=/rd2c/
root     88260  0.0  0.0  13728  9292 pts/1    S    01:47   0:00 /usr/local/bin/python3 -c #! /usr/bin/env python # encoding: utf-8 # WARN
root     88261  0.0  0.0  13728  9644 pts/1    S    01:47   0:00 /usr/local/bin/python3 -c #! /usr/bin/env python # encoding: utf-8 # WARN
root     88262  0.0  0.0  13728  9500 pts/1    S    01:47   0:00 /usr/local/bin/python3 -c #! /usr/bin/env python # encoding: utf-8 # WARN
root     88263  0.0  0.0  13728  9320 pts/1    S    01:47   0:00 /usr/local/bin/python3 -c #! /usr/bin/env python # encoding: utf-8 # WARN
root     88264  0.0  0.0  13728  9604 pts/1    S    01:47   0:00 /usr/local/bin/python3 -c #! /usr/bin/env python # encoding: utf-8 # WARN
root     88265  0.0  0.0  13728  9344 pts/1    S    01:47   0:00 /usr/local/bin/python3 -c #! /usr/bin/env python # encoding: utf-8 # WARN
root     88267 68.7  1.7 731200 248460 pts/1   Sl   01:47  88:52 /rd2c/ns-3-dev/build/scratch/ns3-helics-grid-dnp3-5G --helicsConfig=/rd2c
root     99995  0.0  0.0   8648  3316 pts/1    R+   03:56   0:00 ps aux
```

Example when running 5G: ` tail output/ns3-helics-grid-dnp3-5G.log `, when the run is working propoerly:

```
root@97323560c94f:/rd2c/integration/control# tail output/ns3-helics-grid-dnp3-5G.log 
2024-09-04 16:25:12  MS (   10) Warn: Rx Unauth Seg = 9589
2024-09-04 16:25:12  MS (    9) Warn: Rx Unauth Seg = 15959
2024-09-04 16:25:12  MS (    9) Warn: Rx Unauth Seg = 15960
2024-09-04 16:25:12  MS (    9) Warn: Rx Unauth Seg = 15961
2024-09-04 16:25:12  MS (    9) Warn: Rx Unauth Seg = 15962
2024-09-04 16:25:12  MS (    9) Warn: Rx Unauth Seg = 15963
2024-09-04 16:25:12  MS (   12) Warn: Rx Unauth Seg = 11400
2024-09-04 16:25:12  MS (   12) Warn: Rx Unauth Seg = 11401
2024-09-04 16:25:12  MS (   12) Warn: Rx Unauth Seg = 11402
2024-09-04 16:25:12  MS (   12) Warn: Rx Unauth Seg = 11403
```

` tail output/gridlabd.log `, when the run is working properly:

```
root@97323560c94f:/rd2c/integration/control# tail output/gridlabd.log 
WARNING  [2001-08-01 13:00:02 PDT] : inverter:587 - inv_bat_battery1 - Real power output (2051.28) exceeds maximum DC output (0)
WARNING  [2001-08-01 13:00:02 PDT] : last warning message was repeated 1 times
WARNING  [2001-08-01 13:00:02 PDT] : transformer:xf_t2001014b is at 113.35% of its rated power value
WARNING  [2001-08-01 13:00:02 PDT] : inverter:587 - inv_bat_battery1 - Real power output (2051.28) exceeds maximum DC output (0)
WARNING  [2001-08-01 13:00:02 PDT] : last warning message was repeated 1 times
WARNING  [2001-08-01 13:00:02 PDT] : transformer:xf_t2001014b is at 113.32% of its rated power value
WARNING  [2001-08-01 13:00:03 PDT] : inverter:587 - inv_bat_battery1 - Real power output (2051.28) exceeds maximum DC output (0)
WARNING  [2001-08-01 13:00:03 PDT] : last warning message was repeated 1 times
WARNING  [2001-08-01 13:00:03 PDT] : transformer:xf_t2001014b is at 113.32% of its rated power value
```

` tail output/helics_broker.log `, when the run is working properly:

```
root@97323560c94f:/rd2c/integration/control# tail output/helics_broker.log 
root@97323560c94f:/rd2c/integration/control#
```
