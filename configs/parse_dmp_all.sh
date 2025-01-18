# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/bfs/MAA/22_new --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/bfs/BASE/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/bfs/DMP/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/pr/MAA/22 --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/pr/BASE/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/pr/DMP/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/sssp/MAA/22 --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/sssp/BASE/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/sssp/DMP/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/bc/MAA/22_nolog --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/bc/BASE/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/bc/DMP/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/isb/MAA --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/isb/BASE --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/isb/DMP --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/cgc/MAA/ --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/cgc/BASE/ --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/cgc/DMP/ --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_uniformgraph/PRH/MAA/2M --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_uniformgraph/PRH/BASE/2M --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_uniformgraph/PRH/DMP/2M --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_uniformgraph/PRO/MAA/2M --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_uniformgraph/PRO/BASE/2M --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_uniformgraph/PRO/DMP/2M --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_static/spatter/xrage/MAA --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/spatter/xrage/BASE --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/spatter/xrage/DMP --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_uniformgraph/gradzatp/MAA/2M --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatp/BASE/2M_atom --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatp/DMP/2M_atom --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_uniformgraph/gradzatz/MAA/2M --mode maa --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatz/BASE/2M_atom --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatz/DMP/2M_atom --mode base --target 1
# # python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatp_invert/MAA/2M --mode maa --target 1
# # python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatp_invert/BASE/2M --mode base --target 1
# # python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatp_invert/DMP/2M --mode base --target 1
# # python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatz_invert/MAA/2M --mode maa --target 1
# # python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatz_invert/BASE/2M --mode base --target 1
# # python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_new/gradzatz_invert/DMP/2M --mode base --target 1

# I0_IND_AvgWordsPerCacheLine
# I0_IND_AvgCacheLinesPerRow

# cat /data4/arkhadem/gem5-hpc/results_uniformgraph/gradzatp/MAA/2M/stats.txt | grep -m 1 "AvgWordsPerCacheLine"
# cat /data4/arkhadem/gem5-hpc/results_uniformgraph/gradzatp/MAA/2M/stats.txt | grep -m 1 "AvgCacheLinesPerRow"
# cat /data4/arkhadem/gem5-hpc/results_uniformgraph/gradzatz/MAA/2M/stats.txt | grep -m 1 "AvgWordsPerCacheLine"
# cat /data4/arkhadem/gem5-hpc/results_uniformgraph/gradzatz/MAA/2M/stats.txt | grep -m 1 "AvgCacheLinesPerRow"
# cat /data4/arkhadem/gem5-hpc/results_new/gradzatp_invert/MAA/2M/stats.txt | grep -m 1 "AvgWordsPerCacheLine"
# cat /data4/arkhadem/gem5-hpc/results_new/gradzatp_invert/MAA/2M/stats.txt | grep -m 1 "AvgCacheLinesPerRow"
# cat /data4/arkhadem/gem5-hpc/results_new/gradzatz_invert/MAA/2M/stats.txt | grep -m 1 "AvgWordsPerCacheLine"
# cat /data4/arkhadem/gem5-hpc/results_new/gradzatz_invert/MAA/2M/stats.txt | grep -m 1 "AvgCacheLinesPerRow"

echo "bfs"
cat /data4/arkhadem/gem5-hpc/results_new/bfs/MAA/22_new/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "pr"
cat /data4/arkhadem/gem5-hpc/results_new/pr/MAA/22/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "sssp"
cat /data4/arkhadem/gem5-hpc/results_new/sssp/MAA/22/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "bc"
cat /data4/arkhadem/gem5-hpc/results_new/bc/MAA/22_nolog/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "isb"
cat /data4/arkhadem/gem5-hpc/results/isb/MAA/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "cgc"
cat /data4/arkhadem/gem5-hpc/results_new/cgc/MAA//stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "PRH"
cat /data4/arkhadem/gem5-hpc/results_uniformgraph/PRH/MAA/2M/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "PRO"
cat /data4/arkhadem/gem5-hpc/results_uniformgraph/PRO/MAA/2M/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "spatter"
cat /data4/arkhadem/gem5-hpc/results_static/spatter/xrage/MAA/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "gradzatp"
cat /data4/arkhadem/gem5-hpc/results_uniformgraph/gradzatp/MAA/2M/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "gradzatz"
cat /data4/arkhadem/gem5-hpc/results_uniformgraph/gradzatz/MAA/2M/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "gradzatp_invert"
cat /data4/arkhadem/gem5-hpc/results_new/gradzatp_invert/MAA/2M/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"
echo "gradzatz_invert"
cat /data4/arkhadem/gem5-hpc/results_new/gradzatz_invert/MAA/2M/stats.txt | grep -m 2 -E "AvgWordsPerCacheLine|AvgCacheLinesPerRow"

# echo "bfs"
# cat /data4/arkhadem/gem5-hpc/results_new/bfs/BASE/22/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "pr"
# cat /data4/arkhadem/gem5-hpc/results_new/pr/BASE/22/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "sssp"
# cat /data4/arkhadem/gem5-hpc/results_new/sssp/BASE/22/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "bc"
# cat /data4/arkhadem/gem5-hpc/results_new/bc/BASE/22/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "isb"
# cat /data4/arkhadem/gem5-hpc/results/isb/BASE/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "cgc"
# cat /data4/arkhadem/gem5-hpc/results_new/cgc/BASE//stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "PRH"
# cat /data4/arkhadem/gem5-hpc/results_uniformgraph/PRH/BASE/2M/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "PRO"
# cat /data4/arkhadem/gem5-hpc/results_uniformgraph/PRO/BASE/2M/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "spatter"
# cat /data4/arkhadem/gem5-hpc/results/spatter/xrage/BASE/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "gradzatp"
# cat /data4/arkhadem/gem5-hpc/results_new/gradzatp/BASE/2M_atom/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "gradzatz"
# cat /data4/arkhadem/gem5-hpc/results_new/gradzatz/BASE/2M_atom/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "gradzatp_invert"
# cat /data4/arkhadem/gem5-hpc/results_new/gradzatp_invert/BASE/2M/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"
# echo "gradzatz_invert"
# cat /data4/arkhadem/gem5-hpc/results_new/gradzatz_invert/BASE/2M/stats.txt | grep "switch_cpus0.lsq0.forwLoads_T" # "system.cpu0.dcache.demandAccesses"


# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/bfs/DMP/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/bfs/BASE/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/pr/DMP/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/pr/BASE/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_static/sssp/DMP/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_static/sssp/BASE/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/bc/DMP/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/bc/BASE/22 --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/isb/DMP --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/isb/BASE --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_graphdebug/cgb/DMP/ --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_graphdebug/cgb/BASE/ --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_static/PRH/DMP --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_static/PRH/BASE --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_static/PRO/DMP --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results_static/PRO/BASE --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/spatter/flag/DMP --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/spatter/flag/BASE --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/spatter/xrage/DMP --mode base --target 1
# python configs/parse_gem5.py --dir /data4/arkhadem/gem5-hpc/results/spatter/xrage/BASE --mode base --target 1

# cat /data4/arkhadem/gem5-hpc/results/bfs/DMP/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/bfs/BASE/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/pr/DMP/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/pr/BASE/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results_static/sssp/DMP/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results_static/sssp/BASE/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/bc/DMP/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/bc/BASE/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/isb/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/isb/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results_graphdebug/cgb/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results_graphdebug/cgb/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results_static/PRH/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results_static/PRH/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results_static/PRO/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results_static/PRO/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/spatter/flag/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/spatter/flag/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/spatter/xrage/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"
# cat /data4/arkhadem/gem5-hpc/results/spatter/xrage/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.accuracy"

# cat /data4/arkhadem/gem5-hpc/results/bfs/DMP/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/bfs/BASE/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/pr/DMP/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/pr/BASE/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results_static/sssp/DMP/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results_static/sssp/BASE/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/bc/DMP/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/bc/BASE/22/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/isb/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/isb/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results_graphdebug/cgb/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results_graphdebug/cgb/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results_static/PRH/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results_static/PRH/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results_static/PRO/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results_static/PRO/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/spatter/flag/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/spatter/flag/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/spatter/xrage/DMP/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"
# cat /data4/arkhadem/gem5-hpc/results/spatter/xrage/BASE/stats.txt | grep -m 1 "system.cpu0.dcache.prefetcher.coverage"