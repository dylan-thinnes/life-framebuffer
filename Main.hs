module Main where

import Data.List (intercalate)

-- colors
ansi :: [Int] -> String
ansi xs = "\ESC[" ++ intercalate ";" (map show xs) ++ "m"

color :: Bool -> (Int, Int, Int) -> String -> String
color bg (r, g, b) text = ansi [if bg then 48 else 38, 2, r, g, b] ++ text ++ ansi [0]

tile :: (Int, Int, Int) -> String
tile rgb = color True rgb " "

fromBlack :: (Int, Int, Int) -> Double -> (Int, Int, Int)
fromBlack = linear (0,0,0)

grey, red :: Double -> (Int, Int, Int)
grey = fromBlack (256,256,256)
red = fromBlack (256,0,0)

linear :: (Int, Int, Int) -> (Int, Int, Int) -> Double -> (Int, Int, Int)
linear (r1, g1, b1) (r2, g2, b2) pct = (blend r1 r2, blend g1 g2, blend b1 b2)
  where
    blend start end
      = max 0 $ min 255
      $ start + round (pct * fromIntegral (end - start))

-- conway
combine = zipWith . zipWith
rot n xs = take (length xs) $ drop (length xs + n) (cycle xs)
rot2 x y = rot y . map (rot x)
neighbours field =
  foldr1 (combine (+))
    [ rot2 x y field
    | x <- [-1,0,1]
    , y <- [-1,0,1]
    , x /= 0 || y /= 0
    ]
living = map (map (fromEnum . (== 1)))
step field = combine f field (neighbours (living field))
  where f _ 3 = 1
        f 0 _ = 0
        f 1 2 = 1
        f n _ = n + 1

-- print
pretty :: [[Integer]] -> String
pretty = unlines . (map . concatMap) (\n -> tile $ fromBlack (256,0,128) $ sqrt $ 1 / fromIntegral n)

play :: [[Integer]] -> IO ()
play field = do
  putStrLn "\ESC[2J"
  putStrLn $ pretty field
  _ <- getLine
  play $ step field

-- fields
glider x y = extendY y $ extendX x <$> [[0,1,0],[0,0,1],[1,1,1]]

extendX n row = row ++ replicate (n - length row) 0
extendY n rows = rows ++ replicate (n - length rows) (replicate (length $ head rows) 0)

fromFile path = do
  src <- lines <$> readFile path
  let parsed = (map . map) (fromIntegral . fromEnum . (/= ' ')) src
  let extended = map (extendX (maximum $ map length parsed)) parsed
  pure extended

main = play =<< fromFile "ex"
