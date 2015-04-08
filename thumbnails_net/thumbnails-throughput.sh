#! /usr/bin/env bash

BLAST_PATH="../../parlib-benchmarks/support"

SLEEP_TIME="30"
DATA_DIR="data"
declare -a configs=('linux'
                    'upthread'
                    'upthread-lithe'
                    'upthread-native-omp'
                    'upthread-lithe-native-omp');
declare -a pids=()

mykill() {
	kill -9 $@ 2>/dev/null
	for pid in $@; do
		wait $pid 2>/dev/null
	done
}

launch_thumbnails() {
	local outfile=$1
	echo "  launch_thumbnails: $(date +%s)"
	./thumbnails lights.jpg 100 32 c99:8080/generate_thumbnails_serial > $outfile &
	THUMBNAILS_PID=$!
	pids+=("$THUMBNAILS_PID")
	trap "mykill ${pids[*]}; exit" SIGHUP SIGINT SIGTERM
}

launch_blast() {
	local outfile=$1
	echo "  launch_blast: $(date +%s)"
	${BLAST_PATH}/blast 169.229.49.199 8080 / -1 100 1000 100 > $outfile &
	BLAST_PID=$!
	pids+=("$BLAST_PID")
	trap "mykill ${pids[*]}; exit" SIGHUP SIGINT SIGTERM
}

kill_thumbnails() {
        echo "  kill_thumbnails: $(date +%s)"
	mykill $THUMBNAILS_PID
}

kill_blast() {
        echo "  kill_blast: $(date +%s)"
	mykill $BLAST_PID
}

mkdir -p ${DATA_DIR}
rm -rf "*.tgz"
for config in ${configs[@]}; do
        echo Configuration: ${config}
	launch_blast ${DATA_DIR}/blast-$config
	sleep ${SLEEP_TIME}
	launch_thumbnails ${DATA_DIR}/thumbnails-$config-1
	sleep ${SLEEP_TIME}
	kill_thumbnails
	sleep ${SLEEP_TIME}
	launch_thumbnails ${DATA_DIR}/thumbnails-$config-2
	sleep ${SLEEP_TIME}
	kill_blast
	sleep ${SLEEP_TIME}
	kill_thumbnails
	OUTPUT=$(curl http://c99.millennium.berkeley.edu:8080/terminate 2>&1)
	sleep 10
done

