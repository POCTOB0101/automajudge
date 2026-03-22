# must be enabled with chmod +x run.sh

export RUNCALLED=1
make
FILECOUNT=$(wc -l < files.c)
echo "back in script, filecount is $FILECOUNT"
for ((i=1; i<=FILECOUNT; i++)); do
    FILE=$(awk -v i="$i" 'NR == i' files.c)
    # get names one at a time and run them
    PROGRAM="LD_PRELOAD=./libmyalloc.so ./$(basename -s .c "$FILE").exe"
    echo "about to run program $PROGRAM"
    eval "$PROGRAM"
done
unset FILECOUNT
unset RUNCALLED
rm files.c #cleanup
