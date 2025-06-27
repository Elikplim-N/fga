-- File: db.sql
-- supabase tables and relationships

CREATE TABLE public.alerts (
  id uuid NOT NULL DEFAULT uuid_generate_v4(),
  device_id uuid NOT NULL,
  alert_type text NOT NULL,
  confidence integer NOT NULL CHECK (confidence >= 0 AND confidence <= 100),
  latitude numeric NOT NULL,
  longitude numeric NOT NULL,
  audio_url text,
  resolved boolean NOT NULL DEFAULT false,
  resolved_by uuid,
  resolved_at timestamp with time zone,
  created_at timestamp with time zone DEFAULT now(),
  battery_level integer,
  vibration boolean,
  recorded_at timestamp with time zone DEFAULT now(),
  CONSTRAINT alerts_pkey PRIMARY KEY (id),
  CONSTRAINT alerts_resolved_by_fkey FOREIGN KEY (resolved_by) REFERENCES public.profiles(id),
  CONSTRAINT alerts_device_id_fkey FOREIGN KEY (device_id) REFERENCES public.devices(id)
);
CREATE TABLE public.deployments (
  id uuid NOT NULL DEFAULT uuid_generate_v4(),
  team_id uuid NOT NULL,
  alert_id uuid,
  forest_id uuid,
  status text NOT NULL DEFAULT 'dispatched'::text CHECK (status = ANY (ARRAY['dispatched'::text, 'on-site'::text, 'completed'::text, 'cancelled'::text])),
  notes text,
  created_at timestamp with time zone DEFAULT now(),
  device_id uuid,
  CONSTRAINT deployments_pkey PRIMARY KEY (id),
  CONSTRAINT deployments_device_id_fkey FOREIGN KEY (device_id) REFERENCES public.devices(id),
  CONSTRAINT deployments_forest_id_fkey FOREIGN KEY (forest_id) REFERENCES public.forests(id),
  CONSTRAINT deployments_team_id_fkey FOREIGN KEY (team_id) REFERENCES public.teams(id),
  CONSTRAINT deployments_alert_id_fkey FOREIGN KEY (alert_id) REFERENCES public.alerts(id)
);
CREATE TABLE public.devices (
  id uuid NOT NULL DEFAULT uuid_generate_v4(),
  device_id text NOT NULL UNIQUE,
  name text NOT NULL,
  forest_id uuid,
  latitude numeric,
  longitude numeric,
  status text NOT NULL DEFAULT 'active'::text CHECK (status = ANY (ARRAY['active'::text, 'inactive'::text, 'maintenance'::text])),
  battery_level integer CHECK (battery_level >= 0 AND battery_level <= 100),
  last_ping timestamp with time zone,
  communication_type text NOT NULL DEFAULT 'lora'::text CHECK (communication_type = ANY (ARRAY['lora'::text, 'gsm'::text, 'satellite'::text])),
  notes text,
  created_at timestamp with time zone DEFAULT now(),
  CONSTRAINT devices_pkey PRIMARY KEY (id),
  CONSTRAINT devices_forest_id_fkey FOREIGN KEY (forest_id) REFERENCES public.forests(id)
);
CREATE TABLE public.forests (
  id uuid NOT NULL DEFAULT uuid_generate_v4(),
  name text NOT NULL,
  location USER-DEFINED,
  description text,
  created_at timestamp with time zone DEFAULT now(),
  CONSTRAINT forests_pkey PRIMARY KEY (id)
);
CREATE TABLE public.notifications (
  id uuid NOT NULL DEFAULT uuid_generate_v4(),
  user_id uuid,
  type text NOT NULL CHECK (type = ANY (ARRAY['alert'::text, 'device'::text, 'team'::text, 'system'::text])),
  title text NOT NULL,
  message text NOT NULL,
  read boolean NOT NULL DEFAULT false,
  link text,
  created_at timestamp with time zone DEFAULT now(),
  CONSTRAINT notifications_pkey PRIMARY KEY (id),
  CONSTRAINT notifications_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.profiles(id)
);
CREATE TABLE public.profiles (
  id uuid NOT NULL,
  full_name text,
  role text NOT NULL DEFAULT 'user'::text CHECK (role = ANY (ARRAY['admin'::text, 'manager'::text, 'user'::text])),
  avatar_url text,
  created_at timestamp with time zone DEFAULT now(),
  CONSTRAINT profiles_pkey PRIMARY KEY (id),
  CONSTRAINT profiles_id_fkey FOREIGN KEY (id) REFERENCES auth.users(id)
);
CREATE TABLE public.reports (
  id uuid NOT NULL DEFAULT uuid_generate_v4(),
  deployment_id uuid,
  alert_id uuid,
  report_type text NOT NULL CHECK (report_type = ANY (ARRAY['incident'::text, 'false_alarm'::text, 'maintenance'::text, 'other'::text])),
  description text NOT NULL,
  created_by uuid,
  created_at timestamp with time zone DEFAULT now(),
  CONSTRAINT reports_pkey PRIMARY KEY (id),
  CONSTRAINT reports_alert_id_fkey FOREIGN KEY (alert_id) REFERENCES public.alerts(id),
  CONSTRAINT reports_deployment_id_fkey FOREIGN KEY (deployment_id) REFERENCES public.deployments(id),
  CONSTRAINT reports_created_by_fkey FOREIGN KEY (created_by) REFERENCES public.profiles(id)
);
CREATE TABLE public.spatial_ref_sys (
  srid integer NOT NULL CHECK (srid > 0 AND srid <= 998999),
  auth_name character varying,
  auth_srid integer,
  srtext character varying,
  proj4text character varying,
  CONSTRAINT spatial_ref_sys_pkey PRIMARY KEY (srid)
);
CREATE TABLE public.team_members (
  id uuid NOT NULL DEFAULT uuid_generate_v4(),
  team_id uuid NOT NULL,
  user_id uuid NOT NULL,
  role text NOT NULL DEFAULT 'member'::text CHECK (role = ANY (ARRAY['leader'::text, 'member'::text])),
  created_at timestamp with time zone DEFAULT now(),
  CONSTRAINT team_members_pkey PRIMARY KEY (id),
  CONSTRAINT team_members_team_id_fkey FOREIGN KEY (team_id) REFERENCES public.teams(id),
  CONSTRAINT team_members_user_id_fkey FOREIGN KEY (user_id) REFERENCES public.profiles(id)
);
CREATE TABLE public.teams (
  id uuid NOT NULL DEFAULT uuid_generate_v4(),
  name text NOT NULL,
  status text NOT NULL DEFAULT 'available'::text CHECK (status = ANY (ARRAY['available'::text, 'deployed'::text, 'resting'::text])),
  current_location USER-DEFINED,
  last_report text,
  created_at timestamp with time zone DEFAULT now(),
  CONSTRAINT teams_pkey PRIMARY KEY (id)
);
CREATE TABLE public.telemetry (
  id bigint NOT NULL DEFAULT nextval('telemetry_id_seq'::regclass),
  device_id uuid NOT NULL,
  alert_type text,
  confidence integer,
  latitude double precision,
  longitude double precision,
  battery_level integer,
  vibration boolean,
  recorded_at timestamp with time zone DEFAULT now(),
  CONSTRAINT telemetry_pkey PRIMARY KEY (id),
  CONSTRAINT telemetry_device_id_fkey FOREIGN KEY (device_id) REFERENCES public.devices(id)
);