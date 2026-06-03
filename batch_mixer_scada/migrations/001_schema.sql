-- Batch Mixer SCADA — initial schema
-- Run: psql -U batchmixer -d batchmixer_prod -f 001_schema.sql

CREATE TABLE IF NOT EXISTS recipes (
    id              SERIAL PRIMARY KEY,
    name            VARCHAR(64)    NOT NULL UNIQUE,
    material_a_kg   NUMERIC(8,3)   NOT NULL,
    material_b_kg   NUMERIC(8,3)   NOT NULL,
    mix_duration_s  INTEGER        NOT NULL,
    target_rpm      SMALLINT       NOT NULL,
    max_pressure_bar NUMERIC(5,2)  NOT NULL DEFAULT 6.0,
    created_at      TIMESTAMPTZ    NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS batch_runs (
    id                  BIGSERIAL PRIMARY KEY,
    recipe_id           INT           NOT NULL REFERENCES recipes(id),
    started_at          TIMESTAMPTZ   NOT NULL DEFAULT now(),
    completed_at        TIMESTAMPTZ,
    status              VARCHAR(16)   NOT NULL DEFAULT 'RUNNING',
    actual_mass_a_kg    NUMERIC(8,3),
    actual_mass_b_kg    NUMERIC(8,3),
    peak_pressure_bar   NUMERIC(5,2),
    notes               TEXT
);

CREATE TABLE IF NOT EXISTS sensor_log (
    ts              TIMESTAMPTZ   NOT NULL,
    batch_run_id    BIGINT        NOT NULL REFERENCES batch_runs(id),
    component       VARCHAR(32)   NOT NULL,
    value           NUMERIC(10,4) NOT NULL,
    unit            VARCHAR(8)    NOT NULL
);

-- Indexes justified by EXPLAIN ANALYZE in docs/DB-INDEXES.md
CREATE INDEX IF NOT EXISTS idx_batch_runs_started_at ON batch_runs(started_at);
CREATE INDEX IF NOT EXISTS idx_sensor_log_run_ts     ON sensor_log(batch_run_id, ts);
CREATE INDEX IF NOT EXISTS idx_sensor_log_comp_ts    ON sensor_log(component, ts);
