-- Seed three standard recipes (Flyway-compatible naming)
INSERT INTO recipes (name, material_a_kg, material_b_kg, mix_duration_s, target_rpm, max_pressure_bar)
VALUES
    ('Concrete C25', 300.000, 180.000, 120, 45, 4.5),
    ('Mortar M10',   200.000, 100.000,  90, 35, 5.0),
    ('Grout G5',     150.000,  75.000,  60, 30, 6.0)
ON CONFLICT (name) DO NOTHING;
