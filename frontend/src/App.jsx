import { useEffect, useState } from 'react';
import { getHealth } from './api.js';
import { loadCity, saveCity } from './city.js';
import AlgorithmsPage from './pages/AlgorithmsPage.jsx';
import BenchmarksPage from './pages/BenchmarksPage.jsx';
import DispatchPage from './pages/DispatchPage.jsx';
import RoutesPage from './pages/RoutesPage.jsx';

const PAGES = [
  { id: 'routes', label: 'Routes' },
  { id: 'dispatch', label: 'Dispatch' },
  { id: 'algorithms', label: 'Algorithms' },
  { id: 'benchmarks', label: 'Benchmarks' },
];

export const pageFromHash = (hash) => PAGES.find((p) => `#/${p.id}` === hash)?.id ?? 'routes';

function useHashPage() {
  const [page, setPage] = useState(() => pageFromHash(window.location.hash));
  useEffect(() => {
    const onChange = () => setPage(pageFromHash(window.location.hash));
    window.addEventListener('hashchange', onChange);
    return () => window.removeEventListener('hashchange', onChange);
  }, []);
  return page;
}

function useServerStatus() {
  const [status, setStatus] = useState('checking');
  useEffect(() => {
    let alive = true;
    const c = new AbortController();
    const check = () => getHealth({ signal: c.signal, timeoutMs: 5000 })
      .then(() => alive && setStatus('up'))
      .catch((e) => { if (alive && !c.signal.aborted) setStatus(e.code === 'unavailable' ? 'noprogram' : 'down'); });
    check();
    const timer = setInterval(check, 15_000);
    return () => { alive = false; c.abort(); clearInterval(timer); };
  }, []);
  return status;
}

export default function App() {
  const page = useHashPage();
  const status = useServerStatus();
  const [city, setCity] = useState(loadCity);
  useEffect(() => { saveCity(city); }, [city]);

  useEffect(() => { document.title = `${PAGES.find((p) => p.id === page)?.label} | ParcelHub`; }, [page]);

  return (
    <>
      <a className="skip" href="#main">Skip to content</a>
      <header className="topbar">
        <a className="brand" href="#/routes" aria-label="ParcelHub home">
          <svg width="30" height="30" viewBox="0 0 32 32" aria-hidden="true">
            <rect width="32" height="32" rx="7" className="brand-tile" />
            <path d="M7 22 L14 12 L19 18 L25 9" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round" />
            <circle cx="25" cy="9" r="3" fill="currentColor" />
          </svg>
          ParcelHub
        </a>
        <nav aria-label="Main">
          {PAGES.map((p) => (
            <a key={p.id} href={`#/${p.id}`} aria-current={p.id === page ? 'page' : undefined}>{p.label}</a>
          ))}
        </nav>
        <span className={`server server-${status}`} role="status">
          {status === 'up' ? 'Server running' : status === 'checking' ? 'Checking server…' : status === 'noprogram' ? 'Server up, C++ program missing' : 'Server not running'}
        </span>
      </header>
      {(status === 'down' || status === 'noprogram') && page !== 'benchmarks' && page !== 'algorithms' && (
        <div className="banner" role="alert">
          {status === 'down'
            ? <>The calculation server isn't running, so routes and dispatch can't be calculated. In the <code>backend</code> folder run <code>npm start</code>.</>
            : <>The server is running but can't find the C++ program. Run <code>build.ps1</code> in the project folder.</>}
        </div>
      )}
      <main id="main">
        {page === 'routes' && <RoutesPage city={city} setCity={setCity} />}
        {page === 'dispatch' && <DispatchPage city={city} setCity={setCity} />}
        {page === 'algorithms' && <AlgorithmsPage />}
        {page === 'benchmarks' && <BenchmarksPage />}
      </main>
    </>
  );
}
