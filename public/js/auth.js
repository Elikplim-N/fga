// js/auth.js
import { supabase } from './supabaseClient.js';

// Debug helper
function debugLog(...args) {
  console.log('[Auth Debug]', ...args);
}

// Helper: guard access and fetch user role
async function guardAccess() {
  const { data: { session }, error: sessionErr } = await supabase.auth.getSession();
  if (sessionErr) {
    debugLog('Session error:', sessionErr.message);
    return;
  }
  if (!session) {
    debugLog('No session found, redirecting to login');
    window.location.href = 'login.html';
    return;
  }

  const user = session.user;
  const { data: profile, error: profileErr } = await supabase
    .from('profiles')
    .select('role')
    .eq('id', user.id)
    .single();

  if (profileErr) {
    debugLog('Profile fetch error:', profileErr.message);
    return;
  }

  const role = profile.role;
  localStorage.setItem('userRole', role);

  if (role !== 'admin') {
    document.querySelectorAll('.admin-only').forEach(el => el.remove());
  }
  if (role === 'admin') {
    document.querySelectorAll('.user-only').forEach(el => el.remove());
  }
  debugLog('Access granted for role:', role);
}

// Listen to auth state changes (e.g., sign-out)
supabase.auth.onAuthStateChange((event, session) => {
  debugLog('Auth event:', event);
  if (event === 'SIGNED_OUT') {
    debugLog('User signed out, redirecting to login');
    window.location.href = 'login.html';
  }
});

// Initialize once DOM is ready
document.addEventListener('DOMContentLoaded', () => {
  const path = window.location.pathname;
  // Only guard access on protected pages
  if (!path.endsWith('login.html') && !path.endsWith('register.html') && !path.endsWith('index.html')) {
    guardAccess();
  }

  // — LOGIN
  const loginForm = document.getElementById('login-form');
  if (loginForm) {
    loginForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      const email    = e.target.email.value;
      const password = e.target.password.value;
      const { error } = await supabase.auth.signInWithPassword({ email, password });
      if (error) {
        if (error.message.includes('email not confirmed')) {
          alert('Please verify your email before logging in.');
        } else {
          alert(error.message);
        }
        return;
      }
      debugLog('Login successful, redirecting to dashboard');
      window.location.href = 'dashboard.html';
    });
  }

  // — SIGN UP
  const registerForm = document.getElementById('register-form');
  if (registerForm) {
    registerForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      const full_name = e.target.full_name.value;
      const email     = e.target.email.value;
      const password  = e.target.password.value;

      const { data, error } = await supabase.auth.signUp(
        { email, password },
        { data: { full_name } }
      );
      if (error) {
        alert(error.message);
        return;
      }
      await supabase.from('profiles').insert([{ id: data.user.id, full_name, role: 'user' }]);
      alert('Confirmation email sent! Please verify before logging in.');
      window.location.href = 'login.html';
    });
  }

  // — LOGOUT
  const logoutBtn = document.getElementById('logout-btn');
  console.log('Logout:');
  debugLog('Logout button element:', logoutBtn);
  if (!logoutBtn) {
    debugLog('No logout button found on page');
  } else {
    logoutBtn.addEventListener('click', async () => {
      debugLog('Logout button clicked');
      const { error } = await supabase.auth.signOut();
      if (error) {
        debugLog('Error during signOut:', error.message);
        alert('Error signing out. Try again.');
      }
      // onAuthStateChange will handle redirect
    });
  }
});
