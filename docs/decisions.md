# Decision log

Record durable engineering decisions here. Track tasks and status in GitHub Issues
and the Project board. A proposal becomes agreed only after group discussion.

## Agreed starting concept

The following records reflect the supplied project brief. They describe design
agreements, not verified hardware behaviour.

| ID | Decision | Reason |
|---|---|---|
| D01 | Pi is the Wi-Fi AP/router; receiver is a separate wired computer | Makes forwarding and the future Ethernet queue explicit |
| D02 | Limit the primary claim to controlled gateway QoS | Gateway queueing does not prove Wi-Fi airtime priority |
| D03 | Use `iperf3` for bulk traffic | Provides a reproducible competing workload |
| D04 | Validate fixed QoS before adaptive QoS | Establishes a working comparison before adding policy complexity |
| D05 | Develop incrementally with group review | Keeps the system understandable and changes reviewable |

## Proposed collaboration convention

Markdown stores technical knowledge; Issues store bounded work; one Project board
stores status; pull requests support review and integration. The group should
review this convention with the foundation PR.

## New decision format

```text
ID and title:
Date:
Status: proposed / agreed / superseded
Decision:
Reason and evidence:
Related issue or PR:
```

Next decisions concern the firmware toolchain, addressing/ports, message format,
logging schema, and experiment identifiers. Do not treat earlier suggested values
as agreed until reviewed.
