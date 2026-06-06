-- Aggiunta tabella per allegati task
CREATE TABLE IF NOT EXISTS task_attachments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id INTEGER NOT NULL,
    file_name TEXT NOT NULL,
    file_path TEXT NOT NULL,
    file_type TEXT,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (submission_id) REFERENCES task_submissions(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_attachments_submission ON task_attachments(submission_id);
