@@
 class RecordingAuditSink : public AeroraAuditSink
 {
 public:
-    void record(const AeroraAuditEvent& event) override { events.append(event); }
+    [[nodiscard]] AeroraAuditWriteOutcome record(const AeroraAuditEvent& event) override
+    {
+        events.append(event);
+        return AeroraAuditWriteOutcome::Accepted;
+    }
 
     [[nodiscard]] QString sinkName() const override { return QStringLiteral("Recording"); }
 
     [[nodiscard]] bool hasEventOfType(AeroraAuditEventType type) const
