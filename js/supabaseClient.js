// js/supabaseClient.js
import { createClient } from 'https://cdn.jsdelivr.net/npm/@supabase/supabase-js/+esm';

const SUPABASE_URL     = 'https://xhxzpucbjqtxjvggeigw.supabase.co';
const SUPABASE_ANON_KEY = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InhoeHpwdWNianF0eGp2Z2dlaWd3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDkyMjkwNDIsImV4cCI6MjA2NDgwNTA0Mn0.2HnZGjoZilWVr2S2N-ROqOSNp1XgRBzp6aExPS4EZeQ';

export const supabase = createClient(SUPABASE_URL, SUPABASE_ANON_KEY);
