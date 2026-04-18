#!/usr/bin/env python3
import argparse
import asyncio
import random
import statistics
import struct
import time
from dataclasses import dataclass, field
from typing import List


HEADER_LEN = 4


def encode_msg(msg: bytes) -> bytes:
    """Encode message with 4-byte big-endian length header."""
    return struct.pack("!I", len(msg)) + msg


async def recv_exact(reader: asyncio.StreamReader, n: int) -> bytes:
    """Read exactly n bytes or raise IncompleteReadError."""
    return await reader.readexactly(n)


async def recv_packet(reader: asyncio.StreamReader, max_body: int) -> bytes:
    """Receive one framed packet."""
    header = await recv_exact(reader, HEADER_LEN)
    body_len = struct.unpack("!I", header)[0]
    if body_len > max_body:
        raise ValueError(f"body too large: {body_len}")
    if body_len == 0:
        return b""
    return await recv_exact(reader, body_len)


@dataclass
class WorkerStats:
    sent: int = 0
    ok: int = 0
    failed: int = 0
    bytes_sent: int = 0
    bytes_recv: int = 0
    latencies_us: List[float] = field(default_factory=list)


@dataclass
class SummaryStats:
    sent: int = 0
    ok: int = 0
    failed: int = 0
    bytes_sent: int = 0
    bytes_recv: int = 0
    latencies_us: List[float] = field(default_factory=list)

    def merge(self, other: WorkerStats) -> None:
        self.sent += other.sent
        self.ok += other.ok
        self.failed += other.failed
        self.bytes_sent += other.bytes_sent
        self.bytes_recv += other.bytes_recv
        self.latencies_us.extend(other.latencies_us)


def percentile(sorted_data: List[float], p: float) -> float:
    """Return percentile value from sorted data."""
    if not sorted_data:
        return 0.0
    if len(sorted_data) == 1:
        return sorted_data[0]
    k = (len(sorted_data) - 1) * p
    f = int(k)
    c = min(f + 1, len(sorted_data) - 1)
    if f == c:
        return sorted_data[f]
    d0 = sorted_data[f] * (c - k)
    d1 = sorted_data[c] * (k - f)
    return d0 + d1


def build_payload(args: argparse.Namespace, worker_id: int, seq: int) -> bytes:
    """Build one request payload."""
    if args.heartbeat_ratio > 0 and random.random() < args.heartbeat_ratio:
        return b"__ping__"

    if args.random_size:
        body_size = random.randint(args.min_size, args.max_size)
    else:
        body_size = args.size

    prefix = f"worker={worker_id},seq={seq},".encode()
    if body_size <= len(prefix):
        return prefix[:body_size]

    remain = body_size - len(prefix)
    body = b"x" * remain
    return prefix + body


async def worker(
    worker_id: int,
    host: str,
    port: int,
    duration: float,
    requests_per_conn: int,
    max_body: int,
    args: argparse.Namespace,
) -> WorkerStats:
    """Single connection worker."""
    stats = WorkerStats()
    start_time = time.perf_counter()

    try:
        reader, writer = await asyncio.open_connection(host, port)
    except Exception:
        stats.failed += requests_per_conn if requests_per_conn > 0 else 1
        return stats

    seq = 0
    try:
        while True:
            now = time.perf_counter()
            if now - start_time >= duration:
                break
            if requests_per_conn > 0 and seq >= requests_per_conn:
                break

            payload = build_payload(args, worker_id, seq)
            packet = encode_msg(payload)

            t0 = time.perf_counter()
            try:
                writer.write(packet)
                await writer.drain()

                stats.sent += 1
                stats.bytes_sent += len(packet)

                resp = await recv_packet(reader, max_body)
                t1 = time.perf_counter()

                stats.ok += 1
                stats.bytes_recv += HEADER_LEN + len(resp)
                stats.latencies_us.append((t1 - t0) * 1_000_000.0)

                if args.verify_echo:
                    if payload == b"__ping__":
                        if resp != b"__pong__":
                            stats.failed += 1
                    else:
                        if resp != payload:
                            stats.failed += 1

            except Exception:
                stats.failed += 1
                break

            seq += 1

            if args.interval_ms > 0:
                await asyncio.sleep(args.interval_ms / 1000.0)

    finally:
        writer.close()
        try:
            await writer.wait_closed()
        except Exception:
            pass

    return stats


async def run_benchmark(args: argparse.Namespace) -> None:
    """Run all workers and print summary."""
    tasks = []
    begin = time.perf_counter()

    for i in range(args.connections):
        tasks.append(
            asyncio.create_task(
                worker(
                    worker_id=i,
                    host=args.host,
                    port=args.port,
                    duration=args.duration,
                    requests_per_conn=args.requests_per_conn,
                    max_body=args.max_body,
                    args=args,
                )
            )
        )

    results = await asyncio.gather(*tasks)
    end = time.perf_counter()

    total = SummaryStats()
    for item in results:
        total.merge(item)

    elapsed = max(end - begin, 1e-9)
    lat_sorted = sorted(total.latencies_us)

    avg_us = statistics.mean(lat_sorted) if lat_sorted else 0.0
    p50_us = percentile(lat_sorted, 0.50)
    p95_us = percentile(lat_sorted, 0.95)
    p99_us = percentile(lat_sorted, 0.99)
    max_us = max(lat_sorted) if lat_sorted else 0.0
    qps = total.ok / elapsed
    throughput_out = total.bytes_sent / elapsed
    throughput_in = total.bytes_recv / elapsed

    print("=" * 72)
    print("Benchmark Summary")
    print("=" * 72)
    print(f"host                : {args.host}")
    print(f"port                : {args.port}")
    print(f"connections         : {args.connections}")
    print(f"duration_s          : {elapsed:.3f}")
    print(f"requests_sent       : {total.sent}")
    print(f"requests_ok         : {total.ok}")
    print(f"requests_failed     : {total.failed}")
    print(f"qps                 : {qps:.2f}")
    print(f"bytes_sent_total    : {total.bytes_sent}")
    print(f"bytes_recv_total    : {total.bytes_recv}")
    print(f"bytes_sent_per_sec  : {throughput_out:.2f}")
    print(f"bytes_recv_per_sec  : {throughput_in:.2f}")
    print(f"latency_avg_us      : {avg_us:.2f}")
    print(f"latency_p50_us      : {p50_us:.2f}")
    print(f"latency_p95_us      : {p95_us:.2f}")
    print(f"latency_p99_us      : {p99_us:.2f}")
    print(f"latency_max_us      : {max_us:.2f}")
    print("=" * 72)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Async Python load tester for your echo server.")
    parser.add_argument("--host", default="127.0.0.1", help="Server host")
    parser.add_argument("--port", type=int, default=8080, help="Server port")
    parser.add_argument("--connections", type=int, default=50, help="Concurrent TCP connections")
    parser.add_argument("--duration", type=float, default=10.0, help="Benchmark duration in seconds")
    parser.add_argument("--requests-per-conn", type=int, default=0,
                        help="Max requests per connection, 0 means unlimited until duration ends")
    parser.add_argument("--size", type=int, default=32, help="Fixed payload size for normal messages")
    parser.add_argument("--random-size", action="store_true", help="Use random payload size")
    parser.add_argument("--min-size", type=int, default=1, help="Min random payload size")
    parser.add_argument("--max-size", type=int, default=256, help="Max random payload size")
    parser.add_argument("--interval-ms", type=float, default=0.0,
                        help="Sleep between requests on each connection")
    parser.add_argument("--max-body", type=int, default=1024 * 1024, help="Max response body size")
    parser.add_argument("--heartbeat-ratio", type=float, default=0.0,
                        help="Probability of sending __ping__ instead of normal payload")
    parser.add_argument("--verify-echo", action="store_true",
                        help="Verify normal payload echoes and ping->pong")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.random_size and args.min_size > args.max_size:
        raise ValueError("min-size must be <= max-size")
    asyncio.run(run_benchmark(args))


if __name__ == "__main__":
    main()