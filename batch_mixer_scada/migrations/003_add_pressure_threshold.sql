-- CR-001: Add per-recipe pressure trip threshold
-- Idempotent: checks column existence before ALTER.
-- Rollback: ALTER TABLE recipes DROP COLUMN max_pressure_bar

DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns
        WHERE table_name = 'recipes' AND column_name = 'max_pressure_bar'
    ) THEN
        ALTER TABLE recipes
            ADD COLUMN max_pressure_bar NUMERIC(5,2) NOT NULL DEFAULT 6.0;
    END IF;
END
$$;

UPDATE recipes SET max_pressure_bar = 4.5 WHERE name = 'Concrete C25';
