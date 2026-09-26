# Event investigation: <event>

- Game version / build source:
- Date / investigator:
- Mod commit / CE and MCP versions:
- Local evidence directory: `research/events/<event>/<version>/`
- Status: candidate / verified for listed scenarios / rejected

## Event definition

- Player action or game trigger:
- State or transition to observe:
- Expected timing and acceptable polling delay:
- Positive cases:
- Negative cases (similar actions that must not trigger the event):

## Candidates and evidence

| Candidate | Object identity / field | Code site / signature | Evidence | Result |
| --- | --- | --- | --- | --- |
| | | | | Not checked |

Record module-relative RVAs and pointer traversal separately from temporary heap
addresses. Include captured instruction bytes and the reason for any masked bytes.
Do not commit personal paths or raw process dumps.

## Validation

| Scenario | Expected | Observed | Report / trace |
| --- | --- | --- | --- |
| Trigger the event | | | |
| Similar action without the event | | | |
| Repeat / rapid transitions | | | |
| Load / teleport / object replacement | | | |
| Fresh game process | | | |

## Conclusion

- Selected observation and evidence for its meaning:
- Read failures / ambiguity / unknown-state handling:
- Missed transitions or timing limitations:
- Remaining questions:
- Observer integration and tests, if implemented:
