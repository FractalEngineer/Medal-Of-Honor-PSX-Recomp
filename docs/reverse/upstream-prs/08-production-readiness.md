# Report XR readiness without the debug server

**Current:** VR startup inspection relies on the heavyweight debug server.

**Desired:** Write one opt-in readiness receipt after actual XR submission and expose lightweight VR counters in production.

**Validation:** Production MoH build, fresh headless startup and missing-slot refusal pass. The old slot-2 fixture did not load.

[MoH integration](https://github.com/FractalEngineer/Medal-Of-Honor-PSX-Recomp/tree/1957e28a5dc7f5daa79e2b9b6ed867eb06b41ace) | [Source and credit](https://github.com/FractalEngineer/psxrecomp/blob/vr-pr/08-production-readiness/docs/internal/upstream/vr-production-readiness.md)
