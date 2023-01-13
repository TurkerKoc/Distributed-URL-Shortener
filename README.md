``` bash
mkdir -p cmake-build-debug
cd cmake-build-debug
cmake -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 ..
make
```


# Project: URL shortener

The goal of this project is to implement the storage backend for a transactional URL shortener service (c.f. [bit.ly](https://bit.ly)).
The backend should be fault tolerant and ensure that URLs produce a consistent short value.

## Implementation

To keep the mappings between URL and short id consistent, implement the Raft consensus protocol between storage nodes (see Lecture 5).
Our workload is a simple key-value storage, where we transactionally insert a new mapping, if it does not exist already.
For each insert, ship the insert logs to all replicas to ensure a consistent state.
Replicas should, thus, also be able to answer read-only lookups from a short id to the full URL.
Remember to keep these lookups efficient by using an index structure.

## Workload

For the workload, you can use the same [CSV files](https://db.in.tum.de/teaching/ws2223/clouddataprocessing/data/filelist.csv) 
that we used in the last assignments.
You can also use the larger [ClickBench](https://github.com/ClickHouse/ClickBench) [hits](https://datasets.clickhouse.com/hits_compatible/hits.tsv.gz) dataset.

## Deployment

Running and deploying your project should be similar to Assignment 4.
In the containerized environment, be aware that the local container filesystem is stateless.
When shutting down a container (or when it crashes), its local files are not preserved.
However, we want that our shortened URLs are persistent, even if we restart all nodes.

For a local Docker setup, you can use [volumes](https://docs.docker.com/storage/volumes/):
```
docker volume create cbdp-volume
docker run --mount source=cbdp-volume,target=/space ...
```

In Azure Container Instances, [Azure file shares](https://learn.microsoft.com/en-us/azure/container-instances/container-instances-volume-azure-files) have similar semantics:
```
az storage share create --name cbdp-volume ...
az container create --azure-file-volume-share-name cbdp-volume \
   --azure-file-volume-mount-path /space ...
```

The default configuration in Azure only allocates 1GB of memory to your instances.
If your implementation uses more memory for your workload, you can increase the allocated memory when creating an instance.
E.g., you can create an instance with 4GB:
```
az container create --memory 4 ...
```

## Our Questions
* When we will learn old assignments passed or not?
* Does followers can get write request and forward it to the leader?
* How does initialization works?
  * Where do we keep current leader information and node information?
  * Does the number of nodes pre-determined? (Can a new node enter to the already running system?)
* How does end user communicate with the system? Do we need to implement a command line interface?
  * Does the leader waits for write input from end-user?
  * Also does the followers waits for read from end-user?
  * Do we need threads to wait for inputs while updating logs?
* Do we need to process the provided dataset immediately when the program starts? Or do we need to wait for input data from user?
* Does URL shortener need to return only a unique ID? 

## Report Questions

* Describe the design of your system -> to do!
* How does your cluster handle leader crashes? -> raft handles this (new leader election)
   * How long does it take to elect a new leader? -> depends on timeout and implementation also network
   * Measure the impact of election timeouts. Investigate what happens when it gets too short / too long. -> too short = too many leader election (even no leader crash), too long = determining crashed leader will be longer and this will increase write latency
* Analyze the load of your nodes:
   * How much resources do your nodes use? -> will see
   * Where do inserts create most load? -> leader node (writes goes from leader)
   * Do lookups distribute the load evenly among replicas? -> yes
* How many nodes should you use for this system? What are their roles? -> leader and multiple followers
* Measure the latency to generate a new short URL -> will see
   * Analyze where your system spends time during this operation -> waiting ack from followers takes most time.
* Measure the lookup latency to get the URL from a short id -> directly read from map.
* How does your system scale? -> new node learns logs from others
   * Measure the latency with increased data inserted, e.g., in 10% increments of inserted short URLs -> should increase since we wait for ack's from followers
   * Measure the system performance with more nodes -> will se
