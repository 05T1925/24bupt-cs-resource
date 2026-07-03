import random
import socket
import struct
import threading
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
LOG_DIR = ROOT / "report_assets" / "logs"
LOG_DIR.mkdir(parents=True, exist_ok=True)


def encode_qname(name: str) -> bytes:
    return b"".join(bytes([len(part)]) + part.encode("ascii") for part in name.split(".")) + b"\x00"


def build_query(name: str, qtype: int = 1, qid: int | None = None) -> tuple[int, bytes]:
    if qid is None:
        qid = random.randint(0, 0xFFFF)
    header = struct.pack("!HHHHHH", qid, 0x0100, 1, 0, 0, 0)
    question = encode_qname(name) + struct.pack("!HH", qtype, 1)
    return qid, header + question


def skip_name(buf: bytes, off: int) -> int:
    while off < len(buf):
        length = buf[off]
        if length & 0xC0 == 0xC0:
            return off + 2
        if length == 0:
            return off + 1
        off += 1 + length
    return off


def parse_response(buf: bytes) -> dict:
    if len(buf) < 12:
        return {"error": "short response"}
    qid, flags, qd, an, ns, ar = struct.unpack("!HHHHHH", buf[:12])
    rcode = flags & 0x000F
    off = 12
    for _ in range(qd):
        off = skip_name(buf, off)
        off += 4
    answers = []
    for _ in range(an):
        off = skip_name(buf, off)
        if off + 10 > len(buf):
            break
        rtype, rclass, ttl, rdlen = struct.unpack("!HHIH", buf[off:off + 10])
        off += 10
        rdata = buf[off:off + rdlen]
        off += rdlen
        value = ""
        if rtype == 1 and rdlen == 4:
            value = socket.inet_ntoa(rdata)
        elif rtype == 28 and rdlen == 16:
            value = socket.inet_ntop(socket.AF_INET6, rdata)
        elif rtype in {2, 5, 12}:
            value = f"compressed-or-name-data({rdlen} bytes)"
        elif rtype == 15:
            value = f"mx-data({rdlen} bytes)"
        else:
            value = f"rdata({rdlen} bytes)"
        answers.append({"type": rtype, "class": rclass, "ttl": ttl, "value": value})
    return {"id": qid, "rcode": rcode, "qd": qd, "an": an, "ns": ns, "ar": ar, "answers": answers}


def query(name: str, qtype: int = 1, timeout: float = 5.0) -> dict:
    qid, packet = build_query(name, qtype)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    started = time.time()
    try:
        sock.sendto(packet, ("127.0.0.1", 53))
        data, _ = sock.recvfrom(2048)
        elapsed_ms = (time.time() - started) * 1000
        parsed = parse_response(data)
        parsed.update({
            "domain": name,
            "qtype": qtype,
            "request_id": qid,
            "response_id": parsed.get("id"),
            "id_match": parsed.get("id") == qid,
            "elapsed_ms": round(elapsed_ms, 1),
            "raw_len": len(data),
        })
        return parsed
    except Exception as exc:
        return {
            "domain": name,
            "qtype": qtype,
            "request_id": qid,
            "error": str(exc),
            "id_match": False,
        }
    finally:
        sock.close()


def format_result(result: dict) -> str:
    if "error" in result:
        return (
            f"domain={result['domain']} qtype={result['qtype']} "
            f"request_id={result['request_id']} ERROR={result['error']}"
        )
    values = ", ".join(a["value"] for a in result["answers"]) or "-"
    return (
        f"domain={result['domain']} qtype={result['qtype']} "
        f"request_id={result['request_id']} response_id={result['response_id']} "
        f"id_match={result['id_match']} rcode={result['rcode']} an={result['an']} "
        f"answers=[{values}] elapsed_ms={result['elapsed_ms']} raw_len={result['raw_len']}"
    )


def write_log(name: str, lines: list[str]) -> None:
    path = LOG_DIR / name
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    startup = [
        "Command: .\\dnsrelay_gcc_build.exe -d 8.8.8.8 ..\\资料\\dnsrelay.txt",
        "Note: 服务由后台进程启动，标准输出重定向到 startup_output.txt；以下为测试脚本记录的启动上下文。",
        "Expected startup messages: Debug level 1; Name server 8.8.8.8:53; Database using ..\\资料\\dnsrelay.txt; Socket() is OK!; Bind() is OK!; ClientTable init.",
    ]
    startup_file = LOG_DIR / "startup_output.txt"
    if startup_file.exists():
        content = startup_file.read_text(encoding="utf-8", errors="replace").strip()
        if content:
            startup.append("")
            startup.append(content)
        else:
            startup.append("Captured stdout was empty, likely because the background process had not flushed buffered console output.")
    write_log("startup_context_output.txt", startup)

    cases = [
        ("local_ip_test_output.txt", "test1", 1),
        ("block_test_output.txt", "test0", 1),
        ("relay_test_output.txt", "www.example.com", 1),
        ("uppercase_failed_test_output.txt", "WWW.5DSOFT.COM", 1),
    ]
    for filename, domain, qtype in cases:
        result = query(domain, qtype)
        write_log(filename, [
            f"Query: {domain}, QTYPE={qtype}, server=127.0.0.1:53",
            format_result(result),
        ])

    domains = [
        "test1",
        "test2",
        "test0",
        "www.bupt.cn",
        "www.example.com",
        "www.baidu.com",
        "www.5dsoft.com",
        "WWW.5DSOFT.COM",
        "example.com",
        "www.qq.com",
    ]
    results: list[dict | None] = [None] * len(domains)

    def worker(i: int, domain: str) -> None:
        results[i] = query(domain, 1, timeout=8.0)

    threads = [threading.Thread(target=worker, args=(i, d)) for i, d in enumerate(domains)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    lines = ["Concurrent UDP DNS query test: 10 threads, server=127.0.0.1:53"]
    for result in results:
        lines.append(format_result(result or {"domain": "-", "qtype": 1, "request_id": 0, "error": "missing result"}))
    write_log("concurrency_test_output.txt", lines)


if __name__ == "__main__":
    main()
