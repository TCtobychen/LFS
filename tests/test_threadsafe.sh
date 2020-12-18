#! /bin/bash

#! /bin/bash

echo "thread test"
cd testfolder/MyLFS

for i in 1 2 3 4 5
do {
    echo "I'm the thread 1" >> threadsafe.txt
} & done

for j in 1 2 3 4 5
do {
    echo "I'm the thread 2" >> threadsafe.txt
} & done

wait
cat threadsafe.txt
rm threadsafe.txt