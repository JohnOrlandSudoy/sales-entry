import { useState } from 'react';

interface Props {
  value: string;
  onChange: (v: string) => void;
  onSubmit?: () => void;
  placeholder?: string;
  showDecimal?: boolean;
  maxLen?: number;
  label?: string;
  dense?: boolean;
  /** Fill parent height — grid keys grow evenly (no scroll) */
  fillHeight?: boolean;
}

export default function NumericKeypad({
  value,
  onChange,
  onSubmit,
  placeholder = '',
  showDecimal = true,
  maxLen = 15,
  label,
  dense = false,
  fillHeight = false,
}: Props) {
  const [active, setActive] = useState<string | null>(null);

  const press = (key: string) => {
    setActive(key);
    setTimeout(() => setActive(null), 100);
    if (key === 'back') {
      onChange(value.slice(0, -1));
      return;
    }
    if (key === '.') {
      if (value.includes('.')) return;
      onChange(value + '.');
      return;
    }
    if (value.length >= maxLen) return;
    onChange(value + key);
  };

  const keys = showDecimal
    ? [
        ['1', '2', '3'],
        ['4', '5', '6'],
        ['7', '8', '9'],
        ['.', '0', 'back'],
      ]
    : [
        ['1', '2', '3'],
        ['4', '5', '6'],
        ['7', '8', '9'],
        ['', '0', 'back'],
      ];

  const keySizeClass = fillHeight
    ? 'h-full min-h-0 text-lg rounded-md'
    : dense
      ? 'h-[32px] text-base rounded-md'
      : 'h-[40px] text-lg rounded-lg';

  return (
    <div className={`flex flex-col w-full ${fillHeight ? 'h-full min-h-0 gap-1' : 'gap-1'}`}>
      {label && (
        <div className="text-[9px] text-yellow-500 mb-0.5 font-bold uppercase tracking-widest font-mono shrink-0">
          {label}
        </div>
      )}
      <div
        className={`bg-black border border-gray-800 rounded-lg px-2 text-right font-mono text-green-500 overflow-hidden whitespace-nowrap shadow-inner shrink-0 flex items-center justify-end ${
          fillHeight ? 'h-8 text-base' : dense ? 'py-1 text-lg min-h-[30px]' : 'py-1.5 text-xl min-h-[36px]'
        }`}
      >
        {value || <span className="text-gray-700 text-sm">{placeholder}</span>}
      </div>
      <div
        className={`grid grid-cols-3 ${fillHeight ? 'flex-1 min-h-0 gap-1' : dense ? 'gap-0.5 mt-0.5' : 'gap-1 mt-1'}`}
        style={fillHeight ? { gridTemplateRows: 'repeat(4, minmax(0, 1fr))' } : undefined}
      >
        {keys.map((row, rowIdx) =>
          row.map((key) => {
            const isBack = key === 'back';
            const isDot = key === '.';
            const display = isBack ? '\u232B' : key;
            const isActive = active === key;
            const isEmpty = key === '';

            let btnClass = 'bg-gray-900 text-green-500 border border-gray-800';
            if (isBack) btnClass = 'bg-red-950/40 text-red-500 border border-red-900/50';
            if (isDot) btnClass = 'bg-gray-800 text-yellow-500 border border-gray-700';

            return (
              <button
                key={`${rowIdx}-${key || 'empty'}`}
                type="button"
                disabled={isEmpty}
                onClick={() => !isEmpty && press(key)}
                className={`
                  ${keySizeClass} font-bold transition-all duration-75 flex items-center justify-center
                  ${btnClass}
                  ${isEmpty ? 'invisible pointer-events-none' : ''}
                  ${isActive ? 'scale-90 bg-gray-800 brightness-125' : 'hover:bg-gray-800'}
                  active:scale-95
                `}
              >
                {display}
              </button>
            );
          })
        )}
      </div>
      {onSubmit && (
        <button
          type="button"
          onClick={onSubmit}
          className={`shrink-0 bg-green-900/40 border border-green-800 text-green-400 text-xs font-bold uppercase tracking-widest active:scale-95 transition-all hover:bg-green-800/40 ${
            dense ? 'mt-0.5 h-[34px] rounded-md' : 'mt-1 h-[40px] rounded-lg'
          }`}
        >
          ENTER
        </button>
      )}
    </div>
  );
}
