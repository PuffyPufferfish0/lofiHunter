import React, { useState, useEffect, useRef, useCallback } from 'react';
import { Play, Pause, SkipForward, Settings, User } from 'lucide-react';

import { initializeApp } from 'firebase/app';
import { getAuth, signInAnonymously, signInWithCustomToken, onAuthStateChanged } from 'firebase/auth';
import { getFirestore, doc, setDoc, getDoc, onSnapshot } from 'firebase/firestore';

// Initialize Firebase OUTSIDE the component
const firebaseConfig = JSON.parse(__firebase_config);
const app = initializeApp(firebaseConfig);
const auth = getAuth(app);
const db = getFirestore(app);
const appId = typeof __app_id !== 'undefined' ? __app_id : 'default-app-id';

// Simulated Server Catalog (In a real app, this would be fetched from Firestore)
const CATALOG = {
  '00001': { id: '00001', name: 'Cozy Rock', boost: 0.25, price: 10, img: 'https://placehold.co/120x80/6b7280/ffffff?text=Rock' },
  '00002': { id: '00002', name: 'Lava Lamp', boost: 0.35, price: 50, img: 'https://placehold.co/60x140/ef4444/ffffff?text=Lamp' },
  '00003': { id: '00003', name: 'Bonsai Tree', boost: 0.10, price: 100, img: 'https://placehold.co/100x120/10b981/ffffff?text=Bonsai' },
};

const SECONDS_PER_CREDIT = 10; // Fast for testing

export default function App() {
  // --- Auth State ---
  const [user, setUser] = useState(null);
  const [authLoading, setAuthLoading] = useState(true);

  // --- Game State ---
  const [balance, setBalance] = useState(0);
  const [progress, setProgress] = useState(0); // 0.0 to 1.0
  const [isPlaying, setIsPlaying] = useState(false);
  const [activeItems, setActiveItems] = useState([]); // Array of { instanceId, itemId, x, y }
  
  // --- UI State ---
  const [activeTab, setActiveTab] = useState('room'); // 'room' or 'shop'
  const [draggedItem, setDraggedItem] = useState(null);

  const canvasRef = useRef(null);
  const saveTimeoutRef = useRef(null);
  const progressRef = useRef(progress);
  const balanceRef = useRef(balance);

  // Keep refs up to date for the economy loop
  useEffect(() => { progressRef.current = progress; }, [progress]);
  useEffect(() => { balanceRef.current = balance; }, [balance]);

  useEffect(() => {
    const initAuth = async () => {
      try {
        if (typeof __initial_auth_token !== 'undefined' && __initial_auth_token) {
          await signInWithCustomToken(auth, __initial_auth_token);
        } else {
          await signInAnonymously(auth);
        }
      } catch (err) {
        console.error("Auth Error:", err);
      }
    };
    initAuth();

    const unsubscribe = onAuthStateChanged(auth, (currentUser) => {
      setUser(currentUser);
      setAuthLoading(false);
    });
    return () => unsubscribe();
  }, []);

  useEffect(() => {
    if (!user) return;

    const docRef = doc(db, 'artifacts', appId, 'users', user.uid, 'savegame');

    const loadData = async () => {
      try {
        const snapshot = await getDoc(docRef);
        if (snapshot.exists()) {
          const data = snapshot.data();
          setBalance(data.balance || 0);
          setActiveItems(data.activeItems || []);
        } else {
          // New user, give them a starter item
          const starterItem = { instanceId: crypto.randomUUID(), itemId: '00001', x: 100, y: 100 };
          setActiveItems([starterItem]);
          await setDoc(docRef, { balance: 0, activeItems: [starterItem] });
        }
      } catch (err) {
        console.error("Failed to load save:", err);
      }
    };
    
    loadData();
  }, [user]);

  // Debounced save function to sync state to the cloud
  const saveToCloud = useCallback((newBalance, newItems) => {
    if (!user) return;
    
    if (saveTimeoutRef.current) clearTimeout(saveTimeoutRef.current);
    
    saveTimeoutRef.current = setTimeout(async () => {
      try {
        const docRef = doc(db, 'artifacts', appId, 'users', user.uid, 'savegame');
        await setDoc(docRef, {
          balance: newBalance,
          activeItems: newItems,
          lastSaved: new Date().toISOString()
        }, { merge: true });
        console.log("Cloud Save Successful");
      } catch (err) {
        console.error("Cloud Save Failed:", err);
      }
    }, 2000); // Wait 2 seconds after the last action before writing to DB
  }, [user]);

  // Trigger cloud save when critical state changes
  useEffect(() => {
    if (!authLoading && user) {
      saveToCloud(balance, activeItems);
    }
  }, [balance, activeItems, saveToCloud, authLoading, user]);

  // Calculate current multiplier
  const currentMultiplier = 1.0 + activeItems.reduce((total, item) => {
    const catalogItem = CATALOG[item.itemId];
    return total + (catalogItem ? catalogItem.boost : 0);
  }, 0);

  useEffect(() => {
    if (!isPlaying) return;

    const TICK_RATE_MS = 100; // Run 10 times a second
    const interval = setInterval(() => {
      const timePassedSeconds = TICK_RATE_MS / 1000;
      const boostAmount = timePassedSeconds * currentMultiplier;
      
      let newProgress = progressRef.current + (boostAmount / SECONDS_PER_CREDIT);
      
      if (newProgress >= 1.0) {
        setBalance(prev => prev + 1);
        newProgress -= 1.0;
      }
      
      setProgress(newProgress);
    }, TICK_RATE_MS);

    return () => clearInterval(interval);
  }, [isPlaying, currentMultiplier]);

  const handleBuyItem = (catalogId) => {
    const item = CATALOG[catalogId];
    if (balance >= item.price) {
      setBalance(prev => prev - item.price);
      setActiveItems(prev => [
        ...prev,
        { instanceId: crypto.randomUUID(), itemId: catalogId, x: window.innerWidth / 2 - 50, y: window.innerHeight / 2 - 50 }
      ]);
    }
  };

  const handleMouseDown = (e, instanceId) => {
    if (activeTab !== 'room') return;
    const itemIndex = activeItems.findIndex(i => i.instanceId === instanceId);
    if (itemIndex === -1) return;

    // Move to top (end of array) for z-indexing
    const newItems = [...activeItems];
    const [grabbedItem] = newItems.splice(itemIndex, 1);
    newItems.push(grabbedItem);
    setActiveItems(newItems);

    setDraggedItem({
      instanceId,
      offsetX: e.clientX - grabbedItem.x,
      offsetY: e.clientY - grabbedItem.y
    });
  };

  const handleMouseMove = (e) => {
    if (!draggedItem || activeTab !== 'room') return;

    setActiveItems(prev => prev.map(item => {
      if (item.instanceId === draggedItem.instanceId) {
        return {
          ...item,
          x: e.clientX - draggedItem.offsetX,
          y: e.clientY - draggedItem.offsetY
        };
      }
      return item;
    }));
  };

  const handleMouseUp = () => {
    setDraggedItem(null);
  };

  if (authLoading) {
    return (
      <div className="min-h-screen bg-gray-900 flex items-center justify-center text-white">
        <p className="animate-pulse">Connecting to lofiHunter Network...</p>
      </div>
    );
  }

  return (
    <div 
      className="min-h-screen bg-[#1e1e1e] text-gray-200 font-mono overflow-hidden select-none"
      onMouseMove={handleMouseMove}
      onMouseUp={handleMouseUp}
      onMouseLeave={handleMouseUp}
    >
      {/* Background Room Canvas */}
      {activeTab === 'room' && (
        <div ref={canvasRef} className="absolute inset-0 z-0">
          {activeItems.map((item) => {
            const catItem = CATALOG[item.itemId];
            if (!catItem) return null;
            return (
              <div
                key={item.instanceId}
                onMouseDown={(e) => handleMouseDown(e, item.instanceId)}
                className={`absolute cursor-grab active:cursor-grabbing hover:ring-2 hover:ring-white/20 transition-shadow ${draggedItem?.instanceId === item.instanceId ? 'opacity-80' : ''}`}
                style={{ left: item.x, top: item.y }}
                title={`${catItem.name} (+${catItem.boost * 100}% Boost)`}
              >
                <img src={catItem.img} alt={catItem.name} draggable="false" className="pointer-events-none drop-shadow-xl w-32 h-32 object-contain" />
              </div>
            );
          })}
        </div>
      )}

      {/* Top HUD */}
      <div className="absolute top-0 w-full p-4 flex justify-between items-start z-20 pointer-events-none">
        <div className="pointer-events-auto flex items-center space-x-4 bg-black/60 backdrop-blur-md p-3 rounded-xl border border-white/10 shadow-lg">
          <div className="flex flex-col">
            <span className="text-xs text-gray-400">NETWORK STATUS</span>
            <span className="text-sm font-bold text-green-400 flex items-center gap-2">
              <span className="w-2 h-2 rounded-full bg-green-500 animate-pulse"></span>
              SYNCED
            </span>
          </div>
          <div className="h-8 w-px bg-white/20"></div>
          <div className="flex flex-col">
            <span className="text-xs text-gray-400">PLAYER ID</span>
            <span className="text-sm text-gray-300 flex items-center gap-1">
               <User size={14} /> {user?.uid.substring(0, 6)}...
            </span>
          </div>
        </div>

        <div className="pointer-events-auto flex flex-col items-end gap-2">
          <div className="bg-black/60 backdrop-blur-md px-6 py-3 rounded-xl border border-yellow-500/30 shadow-lg flex items-center gap-4">
            <div className="flex flex-col items-end">
              <span className="text-xs text-gray-400 uppercase">Available Credits</span>
              <span className="text-2xl font-black text-yellow-400">{balance} CR</span>
            </div>
          </div>
          <div className="w-48 bg-black/40 rounded-full h-2 border border-white/5 overflow-hidden">
             <div className="bg-yellow-500 h-full transition-all duration-100 ease-linear" style={{ width: `${progress * 100}%` }}></div>
          </div>
        </div>
      </div>

      {/* Music Player & Controls (Foreground) */}
      <div className="absolute bottom-8 left-8 z-20 flex gap-6 pointer-events-none">
        
        {/* Main MP3 Player */}
        <div className="pointer-events-auto w-72 bg-white rounded-[2rem] p-4 shadow-2xl flex flex-col gap-4 border border-gray-200">
           <div className="w-full aspect-square bg-gray-900 rounded-2xl overflow-hidden relative shadow-inner">
              <img src="https://images.unsplash.com/photo-1614624532983-4ce03382d63d?q=80&w=400&auto=format&fit=crop" alt="Album Art" className="w-full h-full object-cover opacity-80" />
              {isPlaying && (
                <div className="absolute top-2 right-2 text-xs bg-green-500/80 text-white px-2 py-1 rounded-md backdrop-blur-sm font-bold">
                  {currentMultiplier.toFixed(2)}x BOOST
                </div>
              )}
           </div>
           
           <div className="flex justify-center items-center gap-8 py-2 text-black">
              <button className="hover:text-gray-500 transition-colors">
                <SkipForward size={24} className="rotate-180" />
              </button>
              <button 
                onClick={() => setIsPlaying(!isPlaying)}
                className="w-14 h-14 rounded-full bg-black text-white flex items-center justify-center hover:scale-105 active:scale-95 transition-all shadow-md"
              >
                {isPlaying ? <Pause size={28} /> : <Play size={28} className="ml-1" />}
              </button>
              <button className="hover:text-gray-500 transition-colors">
                <SkipForward size={24} />
              </button>
           </div>
        </div>

        {/* Tab Navigation */}
        <div className="pointer-events-auto flex flex-col gap-2 justify-end">
           <button 
             onClick={() => setActiveTab('room')}
             className={`px-6 py-3 rounded-xl font-bold transition-all shadow-lg ${activeTab === 'room' ? 'bg-blue-600 text-white' : 'bg-black/60 text-gray-400 hover:bg-black/80'}`}
           >
             My Room
           </button>
           <button 
             onClick={() => setActiveTab('shop')}
             className={`px-6 py-3 rounded-xl font-bold transition-all shadow-lg ${activeTab === 'shop' ? 'bg-purple-600 text-white' : 'bg-black/60 text-gray-400 hover:bg-black/80'}`}
           >
             Terminal Shop
           </button>
        </div>
      </div>

      {/* Shop Overlay */}
      {activeTab === 'shop' && (
        <div className="absolute inset-0 bg-black/80 backdrop-blur-md z-10 flex items-center justify-center p-8">
           <div className="w-full max-w-4xl bg-gray-900 border border-purple-500/30 rounded-2xl shadow-2xl p-8 flex flex-col max-h-[80vh]">
              <div className="flex justify-between items-center mb-8 border-b border-white/10 pb-4">
                <h2 className="text-3xl font-black text-white">THE CATALOG</h2>
                <div className="text-xl text-yellow-400 font-bold">{balance} CR</div>
              </div>
              
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 overflow-y-auto pr-2">
                 {Object.values(CATALOG).map(item => (
                   <div key={item.id} className="bg-black border border-white/5 rounded-xl p-4 flex flex-col hover:border-purple-500/50 transition-colors group">
                      <div className="h-32 bg-gray-800 rounded-lg mb-4 flex items-center justify-center overflow-hidden">
                         <img src={item.img} alt={item.name} className="max-h-full object-contain group-hover:scale-110 transition-transform" />
                      </div>
                      <h3 className="font-bold text-lg text-white mb-1">{item.name}</h3>
                      <p className="text-green-400 text-sm mb-4">+{item.boost * 100}% Earnings Speed</p>
                      
                      <div className="mt-auto flex gap-2">
                        <button 
                          onClick={() => handleBuyItem(item.id)}
                          disabled={balance < item.price}
                          className={`flex-1 py-2 rounded-lg font-bold transition-all ${balance >= item.price ? 'bg-purple-600 hover:bg-purple-500 text-white' : 'bg-gray-800 text-gray-500 cursor-not-allowed'}`}
                        >
                          {balance >= item.price ? `Buy [${item.price} CR]` : `Need ${item.price - balance} CR`}
                        </button>
                      </div>
                   </div>
                 ))}
              </div>
              
              <button 
                onClick={() => setActiveTab('room')}
                className="mt-6 self-end px-6 py-2 bg-gray-700 hover:bg-gray-600 text-white rounded-lg font-bold transition-colors"
              >
                Close Catalog
              </button>
           </div>
        </div>
      )}
    </div>
  );
}